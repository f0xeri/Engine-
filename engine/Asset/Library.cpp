#include "engine/Asset/Library.hpp"

#include "engine/Core/Log.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace Asset
{

namespace
{

uint32_t mipCount(uint32_t width, uint32_t height)
{
    return std::bit_width(std::max(width, height));
}

const cgltf_accessor* findAttribute(const cgltf_primitive& prim, cgltf_attribute_type type)
{
    for (size_t i = 0; i < prim.attributes_count; ++i)
    {
        if (prim.attributes[i].type == type)
        {
            return prim.attributes[i].data;
        }
    }
    return nullptr;
}

} // namespace

Library::Library(GPU::VulkanContext& ctx,
                 GPU::BindlessRegistry& bindless,
                 GPU::UploadContext& uploader,
                 GeometryPool& geometry)
    : _ctx(ctx)
    , _bindless(bindless)
    , _uploader(uploader)
    , _geometry(geometry)
{
    const uint8_t white[4] = {255, 255, 255, 255};
    _whiteTexture = createTexture(white, 1, 1, true);
}

Library::~Library()
{
    for (const Texture& texture : _textures)
    {
        _ctx.device.destroyImageView(texture.view);
        vmaDestroyImage(_ctx.allocator, texture.image, texture.allocation);
    }
}

uint32_t Library::createTexture(const void* rgba, uint32_t width, uint32_t height, bool srgb)
{
    const vk::Format format = srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
    const uint32_t mips = mipCount(width, height);

    const vk::ImageCreateInfo imageInfo({},
                                        vk::ImageType::e2D,
                                        format,
                                        {width, height, 1},
                                        mips,
                                        1,
                                        vk::SampleCountFlagBits::e1,
                                        vk::ImageTiling::eOptimal,
                                        vk::ImageUsageFlagBits::eSampled |
                                            vk::ImageUsageFlagBits::eTransferDst |
                                            vk::ImageUsageFlagBits::eTransferSrc);
    const VkImageCreateInfo rawInfo = imageInfo;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    if (vmaCreateImage(_ctx.allocator, &rawInfo, &allocInfo, &image, &allocation, nullptr) !=
        VK_SUCCESS)
    {
        LOG_ERROR(Asset, "texture image creation failed ({}x{})", width, height);
        std::abort();
    }

    const vk::ImageView view =
        _ctx.device.createImageView({{},
                                     image,
                                     vk::ImageViewType::e2D,
                                     format,
                                     {},
                                     {vk::ImageAspectFlagBits::eColor, 0, mips, 0, 1}});

    _uploader.uploadImage(
        image,
        width,
        height,
        mips,
        {static_cast<const std::byte*>(rgba), static_cast<size_t>(width) * height * 4});

    _textures.push_back({image, allocation, view});
    return _bindless.registerTexture(view);
}

uint32_t Library::loadTexture(const std::filesystem::path& path, bool srgb)
{
    const std::string key = path.generic_string();
    if (const auto it = _textureSlots.find(key); it != _textureSlots.end())
    {
        return it->second;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(key.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
    {
        LOG_WARN(Asset, "texture load failed: {} ({})", key, stbi_failure_reason());
        return _whiteTexture;
    }

    const uint32_t slot =
        createTexture(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), srgb);
    stbi_image_free(pixels);

    _textureSlots.emplace(key, slot);
    return slot;
}

const Model& Library::load(const std::filesystem::path& path)
{
    const std::string key = path.generic_string();
    if (const auto it = _models.find(key); it != _models.end())
    {
        return it->second;
    }

    const cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, key.c_str(), &data) != cgltf_result_success ||
        cgltf_load_buffers(&options, data, key.c_str()) != cgltf_result_success)
    {
        LOG_ERROR(Asset, "glTF load failed: {}", key);
        std::abort();
    }

    Model model;
    const std::filesystem::path directory = path.parent_path();
    std::unordered_map<const cgltf_material*, uint32_t> materialIndices;

    const auto resolveMaterial = [&](const cgltf_material* material) -> uint32_t
    {
        if (const auto it = materialIndices.find(material); it != materialIndices.end())
        {
            return it->second;
        }

        Material result{.baseColorFactor = glm::vec4(1.0f), .albedoTexture = _whiteTexture};
        if (material)
        {
            const auto& pbr = material->pbr_metallic_roughness;
            result.baseColorFactor = glm::make_vec4(pbr.base_color_factor);
            if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image &&
                pbr.base_color_texture.texture->image->uri)
            {
                std::string uri = pbr.base_color_texture.texture->image->uri;
                cgltf_decode_uri(uri.data());
                uri.resize(std::strlen(uri.c_str()));
                result.albedoTexture = loadTexture(directory / uri, true);
            }
        }

        const auto index = static_cast<uint32_t>(model.materials.size());
        model.materials.push_back(result);
        materialIndices.emplace(material, index);
        return index;
    };

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (size_t n = 0; n < data->nodes_count; ++n)
    {
        const cgltf_node& node = data->nodes[n];
        if (!node.mesh)
        {
            continue;
        }

        glm::mat4 world;
        cgltf_node_transform_world(&node, &world[0][0]);

        for (size_t p = 0; p < node.mesh->primitives_count; ++p)
        {
            const cgltf_primitive& prim = node.mesh->primitives[p];
            if (prim.type != cgltf_primitive_type_triangles)
            {
                continue;
            }

            const cgltf_accessor* pos = findAttribute(prim, cgltf_attribute_type_position);
            const cgltf_accessor* normal = findAttribute(prim, cgltf_attribute_type_normal);
            const cgltf_accessor* uv = findAttribute(prim, cgltf_attribute_type_texcoord);
            if (!pos)
            {
                continue;
            }

            vertices.assign(pos->count, {});
            for (size_t i = 0; i < pos->count; ++i)
            {
                Vertex& v = vertices[i];
                cgltf_accessor_read_float(pos, i, &v.position.x, 3);
                if (normal)
                {
                    cgltf_accessor_read_float(normal, i, &v.normal.x, 3);
                }
                if (uv)
                {
                    cgltf_accessor_read_float(uv, i, &v.uv.x, 2);
                }
            }

            if (prim.indices)
            {
                indices.resize(prim.indices->count);
                for (size_t i = 0; i < prim.indices->count; ++i)
                {
                    indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, i));
                }
            }
            else
            {
                indices.resize(pos->count);
                for (uint32_t i = 0; i < pos->count; ++i)
                {
                    indices[i] = i;
                }
            }

            model.submeshes.push_back({.range = _geometry.add(vertices, indices),
                                       .materialIndex = resolveMaterial(prim.material),
                                       .model = world});
        }
    }

    cgltf_free(data);

    LOG_INFO(Asset,
             "loaded {}: {} submeshes, {} materials",
             key,
             model.submeshes.size(),
             model.materials.size());

    return _models.emplace(key, std::move(model)).first->second;
}

} // namespace Asset
