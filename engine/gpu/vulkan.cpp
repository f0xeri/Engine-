#include <cstdint>
#define VMA_IMPLEMENTATION

#include "engine/gpu/vulkan.hpp"

#include "engine/core/log.hpp"

#include "engine/platform/window.hpp"

#include "engine/core/env.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace
{

constexpr const char* ValidationLayer = "VK_LAYER_KHRONOS_validation";
constexpr const uint32_t VK_REQUIRED_VERSION = VK_API_VERSION_1_3;

void initVulkanLoader()
{
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
    const uint32_t loaderVersion = vk::enumerateInstanceVersion();
    if (loaderVersion < VK_REQUIRED_VERSION)
    {
        LOG_ERROR(Vulkan,
                  "loader only supports {}.{}, but 1.3 is required",
                  VK_API_VERSION_MAJOR(loaderVersion),
                  VK_API_VERSION_MINOR(loaderVersion));
        std::abort();
    }
}

bool hasLayer(const char* name)
{
    for (const auto& lp : vk::enumerateInstanceLayerProperties())
        if (strcmp(lp.layerName, name) == 0)
            return true;
    return false;
}

bool validationRequested()
{
    if (!Core::envFlag("ENGINE_VK_VALIDATION").value_or(false))
    {
        return false;
    }

    if (!hasLayer(ValidationLayer))
    {
        LOG_WARN(Vulkan, "validation requested but {} is not installed", ValidationLayer);
        return false;
    }

    return true;
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                               vk::DebugUtilsMessageTypeFlagsEXT,
                                               const vk::DebugUtilsMessengerCallbackDataEXT* data,
                                               void*)
{
    using Severity = vk::DebugUtilsMessageSeverityFlagBitsEXT;

    if (severity == Severity::eError)
    {
        LOG_ERROR(Vulkan, "{}", data->pMessage);
    }
    else if (severity == Severity::eWarning)
    {
        LOG_WARN(Vulkan, "{}", data->pMessage);
    }
    else
    {
        LOG_INFO(Vulkan, "{}", data->pMessage);
    }

    return vk::False;
}

vk::DebugUtilsMessengerCreateInfoEXT debugMessengerInfo()
{
    using Severity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using Type = vk::DebugUtilsMessageTypeFlagBitsEXT;

    return vk::DebugUtilsMessengerCreateInfoEXT({},
                                                Severity::eWarning | Severity::eError,
                                                Type::eGeneral | Type::eValidation |
                                                    Type::ePerformance,
                                                debugCallback);
}

} // namespace

namespace GPU
{

VulkanContext::VulkanContext(const Platform::Window& window)
{
    initVulkanLoader();

    createInstance(window, validationRequested());
    createSurface(window);
    pickPhysicalDevice();
    createDevice();
    createAllocator();
}

VulkanContext::~VulkanContext()
{
    if (device)
    {
        device.waitIdle();
    }

    if (allocator)
    {
        vmaDestroyAllocator(allocator);
    }
    if (device)
    {
        device.destroy();
    }
    if (surface)
    {
        instance.destroySurfaceKHR(surface);
    }
    if (debugMessenger)
    {
        instance.destroyDebugUtilsMessengerEXT(debugMessenger);
    }
    if (instance)
    {
        instance.destroy();
    }
}

void VulkanContext::createInstance(const Platform::Window& window, bool validationEnabled)
{
    const auto platformExtensions = window.requiredInstanceExtensions();

    std::vector<const char*> instanceExts(platformExtensions.begin(), platformExtensions.end());
    std::vector<const char*> layers;

    if (validationEnabled)
    {
        instanceExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        layers.push_back(ValidationLayer);
    }

    const vk::ApplicationInfo appInfo(window.getTitle(), 1, "Engine", 1, VK_REQUIRED_VERSION);
    vk::InstanceCreateInfo createInfo({}, &appInfo, layers, instanceExts);

    // Chaining the messenger info here makes instance creation and destruction
    // itself covered by validation — the messenger object cannot exist yet.
    const auto messengerInfo = debugMessengerInfo();
    if (validationEnabled)
    {
        createInfo.pNext = &messengerInfo;
    }

    instance = vk::createInstance(createInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

    if (validationEnabled)
    {
        createDebugMessenger();
    }

    LOG_INFO(Vulkan, "instance created (validation: {})", validationEnabled ? "on" : "off");
}

void VulkanContext::createDebugMessenger()
{
    debugMessenger = instance.createDebugUtilsMessengerEXT(debugMessengerInfo());
}

void VulkanContext::createSurface(const Platform::Window& window)
{
    surface = window.createSurface(instance);
}

void VulkanContext::pickPhysicalDevice()
{
    for (const vk::PhysicalDevice& candidate : instance.enumeratePhysicalDevices())
    {
        const auto props = candidate.getProperties();
        if (props.apiVersion < VK_REQUIRED_VERSION)
        {
            continue;
        }

        const auto features = candidate.getFeatures2<vk::PhysicalDeviceFeatures2,
                                                     vk::PhysicalDeviceVulkan12Features,
                                                     vk::PhysicalDeviceVulkan13Features>();
        const auto& core = features.get<vk::PhysicalDeviceFeatures2>().features;
        const auto& v12 = features.get<vk::PhysicalDeviceVulkan12Features>();
        const auto& v13 = features.get<vk::PhysicalDeviceVulkan13Features>();

        if (!v13.dynamicRendering || !v13.synchronization2 ||
            !v12.descriptorBindingSampledImageUpdateAfterBind ||
            !v12.descriptorBindingStorageBufferUpdateAfterBind ||
            !v12.descriptorBindingPartiallyBound || !v12.runtimeDescriptorArray ||
            !v12.drawIndirectCount || !core.multiDrawIndirect || !core.samplerAnisotropy)
        {
            LOG_INFO(Vulkan,
                     "skipping {}: incomplete 1.3 feature baseline",
                     static_cast<const char*>(props.deviceName));
            continue;
        }

        // single queue family doing graphics + present
        const auto families = candidate.getQueueFamilyProperties();
        for (uint32_t i = 0; i < families.size(); ++i)
        {
            const bool graphics =
                static_cast<bool>(families[i].queueFlags & vk::QueueFlagBits::eGraphics);
            const bool present = candidate.getSurfaceSupportKHR(i, surface) == vk::True;

            if (graphics && present)
            {
                // prefer a discrete GPU, but accept the first capable one
                const bool discrete = props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
                if (!physicalDevice || discrete)
                {
                    physicalDevice = candidate;
                    deviceProps = props;
                    graphicsQueueFamily = i;
                }
                break;
            }
        }

        if (physicalDevice && deviceProps.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
        {
            break;
        }
    }

    if (!physicalDevice)
    {
        LOG_ERROR(Vulkan, "no device satisfies the Vulkan 1.3 feature baseline");
        std::abort();
    }

    LOG_INFO(Vulkan,
             "device: {} (queue family {})",
             static_cast<const char*>(deviceProps.deviceName),
             graphicsQueueFamily);
}

void VulkanContext::createDevice()
{
    const float queuePriority = 1.0f;
    const vk::DeviceQueueCreateInfo queueInfo({}, graphicsQueueFamily, 1, &queuePriority);

    const char* deviceExts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    vk::PhysicalDeviceFeatures coreFeatures;
    coreFeatures.samplerAnisotropy = vk::True;
    coreFeatures.multiDrawIndirect = vk::True;

    vk::PhysicalDeviceVulkan12Features v12;
    v12.descriptorBindingSampledImageUpdateAfterBind = vk::True;
    v12.descriptorBindingStorageBufferUpdateAfterBind = vk::True;
    v12.descriptorBindingPartiallyBound = vk::True;
    v12.runtimeDescriptorArray = vk::True;
    v12.drawIndirectCount = vk::True;

    vk::PhysicalDeviceVulkan13Features v13;
    v13.dynamicRendering = vk::True;
    v13.synchronization2 = vk::True;

    vk::StructureChain<vk::DeviceCreateInfo,
                       vk::PhysicalDeviceFeatures2,
                       vk::PhysicalDeviceVulkan12Features,
                       vk::PhysicalDeviceVulkan13Features>
        chain{vk::DeviceCreateInfo({}, queueInfo, {}, deviceExts),
              vk::PhysicalDeviceFeatures2(coreFeatures),
              v12,
              v13};

    device = physicalDevice.createDevice(chain.get<vk::DeviceCreateInfo>());

    // Device-level entry points bypass the loader's dispatch trampoline.
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
    graphicsQueue = device.getQueue(graphicsQueueFamily, 0);

    LOG_INFO(Vulkan, "device created");
}

void VulkanContext::createAllocator()
{
    VmaVulkanFunctions functions{};
    functions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo info{};
    info.vulkanApiVersion = VK_REQUIRED_VERSION;
    info.instance = instance;
    info.physicalDevice = physicalDevice;
    info.device = device;
    info.pVulkanFunctions = &functions;

    if (vmaCreateAllocator(&info, &allocator) != VK_SUCCESS)
    {
        LOG_ERROR(Vulkan, "failed to create VMA allocator");
        std::abort();
    }

    LOG_INFO(Vulkan, "allocator created");
}

} // namespace GPU
