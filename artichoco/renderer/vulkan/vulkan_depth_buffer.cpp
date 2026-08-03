#include "vulkan_depth_buffer.h"

#include <array>
#include <stdexcept>

namespace arti::renderer::vulkan {
namespace {

vk::Format chooseDepthFormat(const VulkanDevice& device)
{
    constexpr std::array candidates = {
        vk::Format::eD32Sfloat,
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD24UnormS8Uint,
    };
    for (const vk::Format format : candidates) {
        const auto properties = device.physicalDevice().getFormatProperties(format);
        if (properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            return format;
        }
    }
    throw std::runtime_error("The Vulkan device exposes no supported depth format.");
}

} // namespace

VulkanDepthBuffer::VulkanDepthBuffer(
    const VulkanDevice& device,
    VulkanAllocator& allocator,
    vk::Extent2D extent)
    : m_format(chooseDepthFormat(device))
{
    vk::ImageCreateInfo image_info{};
    image_info.setImageType(vk::ImageType::e2D)
        .setFormat(m_format)
        .setExtent({extent.width, extent.height, 1})
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);
    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    m_image = allocator.createImage(image_info, allocation_info);

    vk::ImageSubresourceRange range{};
    range.setAspectMask(vk::ImageAspectFlagBits::eDepth)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    vk::ImageViewCreateInfo view_info{};
    view_info.setImage(m_image.handle())
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(m_format)
        .setSubresourceRange(range);
    m_image_view = vk::raii::ImageView{device.device(), view_info};
}

vk::Image VulkanDepthBuffer::image() const noexcept
{
    return m_image.handle();
}

const vk::raii::ImageView& VulkanDepthBuffer::imageView() const noexcept
{
    return m_image_view;
}

vk::Format VulkanDepthBuffer::format() const noexcept
{
    return m_format;
}

} // namespace arti::renderer::vulkan
