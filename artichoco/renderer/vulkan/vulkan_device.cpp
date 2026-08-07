#include "vulkan_device.h"

#include "artichoco/renderer/renderer_log.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace arti::renderer::vulkan {
namespace {

struct QueueFamilies {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
};

QueueFamilies findQueueFamilies(const vk::raii::PhysicalDevice& device, vk::SurfaceKHR surface)
{
    QueueFamilies result;
    const auto properties = device.getQueueFamilyProperties();
    for (uint32_t index = 0; index < properties.size(); ++index) {
        if (properties[index].queueCount == 0) {
            continue;
        }

        const bool supports_graphics = static_cast<bool>(properties[index].queueFlags & vk::QueueFlagBits::eGraphics);
        const bool supports_present = static_cast<bool>(device.getSurfaceSupportKHR(index, surface));
        if (supports_graphics && supports_present) {
            return {index, index};
        }
        if (supports_graphics && !result.graphics) {
            result.graphics = index;
        }
        if (supports_present && !result.present) {
            result.present = index;
        }
    }
    return result;
}

bool hasDeviceExtension(const vk::raii::PhysicalDevice& device, std::string_view name)
{
    const auto extensions = device.enumerateDeviceExtensionProperties();
    return std::ranges::any_of(extensions, [name](const vk::ExtensionProperties& extension) {
        return name == extension.extensionName;
    });
}

bool supportsRenderingFeatures(const vk::raii::PhysicalDevice& device)
{
    const auto features = device.getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceDynamicRenderingFeatures,
        vk::PhysicalDeviceSynchronization2Features>();
    return features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
        features.get<vk::PhysicalDeviceDynamicRenderingFeatures>().dynamicRendering &&
        features.get<vk::PhysicalDeviceSynchronization2Features>().synchronization2;
}

bool supportsRenderingExtensions(const vk::raii::PhysicalDevice& device)
{
    if (device.getProperties().apiVersion < VK_API_VERSION_1_1) {
        return false;
    }
    if (device.getProperties().apiVersion >= VK_API_VERSION_1_3) {
        return true;
    }
    return hasDeviceExtension(device, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) &&
        hasDeviceExtension(device, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
}

uint64_t deviceScore(const vk::raii::PhysicalDevice& device, vk::SurfaceKHR surface)
{
    const auto queues = findQueueFamilies(device, surface);
    if (!queues.graphics || !queues.present || !hasDeviceExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME) ||
        !supportsRenderingExtensions(device) || !supportsRenderingFeatures(device)) {
        return 0;
    }
    if (device.getSurfaceFormatsKHR(surface).empty() || device.getSurfacePresentModesKHR(surface).empty()) {
        return 0;
    }

    const auto properties = device.getProperties();
    uint64_t score = properties.limits.maxImageDimension2D;
    if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        score += 1'000'000;
    }
    return score;
}

} // namespace

VulkanDevice::VulkanDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface)
{
    auto physical_devices = instance.enumeratePhysicalDevices();
    if (physical_devices.empty()) {
        throw std::runtime_error("No Vulkan physical devices were found.");
    }

    size_t selected_index = std::numeric_limits<size_t>::max();
    uint64_t selected_score = 0;
    for (size_t index = 0; index < physical_devices.size(); ++index) {
        const uint64_t score = deviceScore(physical_devices[index], *surface);
        if (score > selected_score) {
            selected_score = score;
            selected_index = index;
        }
    }
    if (selected_index == std::numeric_limits<size_t>::max()) {
        throw std::runtime_error("No Vulkan device supports graphics, presentation, and swapchains.");
    }

    m_physical_device = std::move(physical_devices[selected_index]);
    m_uses_core_13 = m_physical_device.getProperties().apiVersion >= VK_API_VERSION_1_3;
    const auto queues = findQueueFamilies(m_physical_device, *surface);
    m_graphics_queue_family = *queues.graphics;
    m_present_queue_family = *queues.present;

    const float queue_priority = 1.0f;
    const std::set<uint32_t> unique_queue_families = {m_graphics_queue_family, m_present_queue_family};
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(unique_queue_families.size());
    for (const uint32_t family : unique_queue_families) {
        vk::DeviceQueueCreateInfo queue_info{};
        queue_info.setQueueFamilyIndex(family).setQueueCount(1).setPQueuePriorities(&queue_priority);
        queue_create_infos.push_back(queue_info);
    }

    std::vector<const char*> extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    if (!m_uses_core_13) {
        extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    }
#if defined(__APPLE__)
    if (hasDeviceExtension(m_physical_device, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
        extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    }
#endif

    vk::PhysicalDeviceSynchronization2Features synchronization2{};
    synchronization2.setSynchronization2(true);
    vk::PhysicalDeviceDynamicRenderingFeatures dynamic_rendering{};
    dynamic_rendering.setDynamicRendering(true).setPNext(&synchronization2);
    vk::PhysicalDeviceVulkan11Features vulkan_11_features{};
    vulkan_11_features.setShaderDrawParameters(true).setPNext(&dynamic_rendering);
    vk::PhysicalDeviceFeatures enabled_features{};
    m_independent_blend_enabled = static_cast<bool>(m_physical_device.getFeatures().independentBlend);
    enabled_features.setIndependentBlend(m_independent_blend_enabled);
    m_sampler_anisotropy_enabled = static_cast<bool>(m_physical_device.getFeatures().samplerAnisotropy);
    enabled_features.setSamplerAnisotropy(m_sampler_anisotropy_enabled);

    vk::DeviceCreateInfo device_info{};
    device_info.setQueueCreateInfos(queue_create_infos)
        .setPEnabledExtensionNames(extensions)
        .setPEnabledFeatures(&enabled_features);
    device_info.setPNext(&vulkan_11_features);
    m_device = vk::raii::Device{m_physical_device, device_info};
    m_graphics_queue = vk::raii::Queue{m_device, m_graphics_queue_family, 0};
    m_present_queue = vk::raii::Queue{m_device, m_present_queue_family, 0};

    const auto properties = m_physical_device.getProperties();
    getLogChannel().info(
        "Selected Vulkan device '{}' (API {}.{}.{})",
        properties.deviceName.data(),
        VK_API_VERSION_MAJOR(properties.apiVersion),
        VK_API_VERSION_MINOR(properties.apiVersion),
        VK_API_VERSION_PATCH(properties.apiVersion));
}

const vk::raii::PhysicalDevice& VulkanDevice::physicalDevice() const noexcept
{
    return m_physical_device;
}

const vk::raii::Device& VulkanDevice::device() const noexcept
{
    return m_device;
}

const vk::raii::Queue& VulkanDevice::graphicsQueue() const noexcept
{
    return m_graphics_queue;
}

const vk::raii::Queue& VulkanDevice::presentQueue() const noexcept
{
    return m_present_queue;
}

uint32_t VulkanDevice::graphicsQueueFamily() const noexcept
{
    return m_graphics_queue_family;
}

uint32_t VulkanDevice::presentQueueFamily() const noexcept
{
    return m_present_queue_family;
}

bool VulkanDevice::usesCore13() const noexcept
{
    return m_uses_core_13;
}

bool VulkanDevice::independentBlendEnabled() const noexcept
{
    return m_independent_blend_enabled;
}

bool VulkanDevice::samplerAnisotropyEnabled() const noexcept
{
    return m_sampler_anisotropy_enabled;
}

} // namespace arti::renderer::vulkan
