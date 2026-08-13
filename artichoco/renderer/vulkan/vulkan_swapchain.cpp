#include "vulkan_swapchain.h"

#include "artichoco/renderer/renderer_log.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace arti::renderer::vulkan {
namespace {

struct SwapchainFormat {
    vk::SurfaceFormatKHR surface;
    vk::Format view{vk::Format::eUndefined};
    bool mutable_format{false};
};

SwapchainFormat chooseSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR>& formats, const VulkanDevice& device)
{
    struct FormatPair {
        vk::Format unorm;
        vk::Format srgb;
    };
    constexpr std::array preferred_pairs = {
        FormatPair{vk::Format::eB8G8R8A8Unorm, vk::Format::eB8G8R8A8Srgb},
        FormatPair{vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Srgb},
    };

    if (device.mutableSwapchainFormatEnabled()) {
        for (const auto [unorm, srgb] : preferred_pairs) {
            const auto it = std::ranges::find_if(formats, [unorm](const vk::SurfaceFormatKHR& format) {
                return format.format == unorm &&
                    format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            });
            if (it == formats.end()) {
                continue;
            }

            const vk::FormatFeatureFlags unorm_features =
                device.physicalDevice().getFormatProperties(unorm).optimalTilingFeatures;
            const vk::FormatFeatureFlags srgb_features =
                device.physicalDevice().getFormatProperties(srgb).optimalTilingFeatures;
            if ((unorm_features & vk::FormatFeatureFlagBits::eStorageImage) &&
                (unorm_features & vk::FormatFeatureFlagBits::eColorAttachment) &&
                (srgb_features & vk::FormatFeatureFlagBits::eColorAttachment)) {
                return {*it, srgb, true};
            }
        }
    }

    for (const auto [unorm, srgb] : preferred_pairs) {
        const auto it = std::ranges::find_if(formats, [srgb](const vk::SurfaceFormatKHR& format) {
            return format.format == srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });
        if (it != formats.end()) {
            return {*it, srgb, false};
        }
    }
    return {formats.front(), formats.front().format, false};
}

vk::PresentModeKHR choosePresentMode(const std::vector<vk::PresentModeKHR>& modes)
{
    return std::ranges::find(modes, vk::PresentModeKHR::eMailbox) != modes.end()
        ? vk::PresentModeKHR::eMailbox
        : vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities, const core::Window& window)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    return {
        std::clamp(window.getFramebufferWidth(), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(window.getFramebufferHeight(), capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };
}

vk::CompositeAlphaFlagBitsKHR chooseCompositeAlpha(vk::CompositeAlphaFlagsKHR supported)
{
    constexpr std::array choices = {
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
        vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
        vk::CompositeAlphaFlagBitsKHR::eInherit,
    };
    for (const auto choice : choices) {
        if (supported & choice) {
            return choice;
        }
    }
    throw std::runtime_error("The Vulkan surface exposes no supported composite alpha mode.");
}

} // namespace

VulkanSwapchain::VulkanSwapchain(core::Window& window, const VulkanDevice& device, const VulkanSurface& surface)
    : m_window(window),
      m_device(device),
      m_surface(surface)
{
    recreate();
}

bool VulkanSwapchain::recreate()
{
    if (m_window.getFramebufferWidth() == 0 || m_window.getFramebufferHeight() == 0) {
        return false;
    }

    const vk::SurfaceKHR surface = *m_surface.handle();
    const auto capabilities = m_device.physicalDevice().getSurfaceCapabilitiesKHR(surface);
    const auto formats = m_device.physicalDevice().getSurfaceFormatsKHR(surface);
    const auto present_modes = m_device.physicalDevice().getSurfacePresentModesKHR(surface);
    if (formats.empty() || present_modes.empty()) {
        throw std::runtime_error("The Vulkan surface has no usable formats or present modes.");
    }
    if (!(capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eColorAttachment)) {
        throw std::runtime_error("The Vulkan surface does not support color attachment images.");
    }

    const auto swapchain_format = chooseSurfaceFormat(formats, m_device);
    const auto& surface_format = swapchain_format.surface;
    const auto present_mode = choosePresentMode(present_modes);
    const auto extent = chooseExtent(capabilities, m_window);
    if (extent.width == 0 || extent.height == 0) {
        return false;
    }
    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        image_count = std::min(image_count, capabilities.maxImageCount);
    }

    const std::array queue_families = {
        m_device.graphicsQueueFamily(),
        m_device.presentQueueFamily(),
    };

    vk::SwapchainCreateInfoKHR create_info{};
    create_info.setSurface(surface)
        .setMinImageCount(image_count)
        .setImageFormat(surface_format.format)
        .setImageColorSpace(surface_format.colorSpace)
        .setImageExtent(extent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setPreTransform(capabilities.currentTransform)
        .setCompositeAlpha(chooseCompositeAlpha(capabilities.supportedCompositeAlpha))
        .setPresentMode(present_mode)
        .setClipped(true)
        .setOldSwapchain(*m_swapchain);

    std::array<vk::Format, 2> view_formats{};
    vk::ImageFormatListCreateInfo format_list{};
    if (swapchain_format.mutable_format) {
        view_formats = {surface_format.format, swapchain_format.view};
        format_list.setViewFormats(view_formats);
        create_info.setFlags(vk::SwapchainCreateFlagBitsKHR::eMutableFormat)
            .setPNext(&format_list);
    }

    if (queue_families[0] != queue_families[1]) {
        create_info.setImageSharingMode(vk::SharingMode::eConcurrent).setQueueFamilyIndices(queue_families);
    } else {
        create_info.setImageSharingMode(vk::SharingMode::eExclusive);
    }

    vk::raii::SwapchainKHR new_swapchain{m_device.device(), create_info};
    auto new_images = new_swapchain.getImages();
    std::vector<vk::raii::ImageView> new_image_views;
    new_image_views.reserve(new_images.size());
    for (const vk::Image image : new_images) {
        vk::ImageSubresourceRange range{};
        range.setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setBaseMipLevel(0)
            .setLevelCount(1)
            .setBaseArrayLayer(0)
            .setLayerCount(1);

        vk::ImageViewCreateInfo view_info{};
        vk::ImageViewUsageCreateInfo view_usage{};
        view_usage.setUsage(vk::ImageUsageFlagBits::eColorAttachment);
        view_info.setImage(image)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(swapchain_format.view)
            .setSubresourceRange(range)
            .setPNext(&view_usage);
        new_image_views.emplace_back(m_device.device(), view_info);
    }

    m_image_views.clear();
    m_images.clear();
    m_swapchain = std::move(new_swapchain);
    m_images = std::move(new_images);
    m_image_views = std::move(new_image_views);
    m_format = swapchain_format.view;
    m_extent = extent;
    m_min_image_count = capabilities.minImageCount;

    getLogChannel().info(
        "Created Vulkan swapchain ({}x{}, {} images, image {}, view {})",
        m_extent.width,
        m_extent.height,
        m_images.size(),
        vk::to_string(surface_format.format),
        vk::to_string(m_format));
    return true;
}

void VulkanSwapchain::invalidate() noexcept
{
    m_image_views.clear();
    m_images.clear();
    m_swapchain = vk::raii::SwapchainKHR{nullptr};
    m_format = vk::Format::eUndefined;
    m_extent = vk::Extent2D{};
    m_min_image_count = 0;
}

bool VulkanSwapchain::isRenderable() const noexcept
{
    return m_extent.width > 0 && m_extent.height > 0 && !m_images.empty();
}

const vk::raii::SwapchainKHR& VulkanSwapchain::handle() const noexcept
{
    return m_swapchain;
}

vk::Image VulkanSwapchain::image(uint32_t index) const
{
    return m_images.at(index);
}

const vk::raii::ImageView& VulkanSwapchain::imageView(uint32_t index) const
{
    return m_image_views.at(index);
}

vk::Format VulkanSwapchain::format() const noexcept
{
    return m_format;
}

vk::Extent2D VulkanSwapchain::extent() const noexcept
{
    return m_extent;
}

uint32_t VulkanSwapchain::minImageCount() const noexcept
{
    return m_min_image_count;
}

size_t VulkanSwapchain::imageCount() const noexcept
{
    return m_images.size();
}

} // namespace arti::renderer::vulkan
