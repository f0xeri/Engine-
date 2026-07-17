#pragma once

#include "engine/Asset/GeometryPool.hpp"
#include "engine/Core/Math.hpp"
#include "engine/GPU/BindlessRegistry.hpp"
#include "engine/GPU/UploadContext.hpp"
#include "engine/GPU/VulkanContext.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Asset
{

struct Material
{
    glm::vec4 baseColorFactor;
    uint32_t albedoTexture; // bindless slot index
};

struct SubMesh
{
    MeshRange range;
    uint32_t materialIndex;
    glm::mat4 model;
};

struct Model
{
    std::vector<SubMesh> submeshes;
    std::vector<Material> materials;
};

class Library
{
public:
    Library(GPU::VulkanContext& ctx,
            GPU::BindlessRegistry& bindless,
            GPU::UploadContext& uploader,
            GeometryPool& geometry);
    ~Library();

    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;

    // cached: loading the same path twice returns the same Model
    const Model& load(const std::filesystem::path& path);

private:
    struct Texture
    {
        vk::Image image;
        VmaAllocation allocation;
        vk::ImageView view;
    };

    uint32_t loadTexture(const std::filesystem::path& path, bool srgb);
    uint32_t createTexture(const void* rgba, uint32_t width, uint32_t height, bool srgb);

    GPU::VulkanContext& _ctx;
    GPU::BindlessRegistry& _bindless;
    GPU::UploadContext& _uploader;
    GeometryPool& _geometry;

    std::unordered_map<std::string, Model> _models;
    std::unordered_map<std::string, uint32_t> _textureSlots;
    std::vector<Texture> _textures;
    uint32_t _whiteTexture = 0; // slot of the 1x1 fallback
};

} // namespace Asset
