#include "vulkan_image.h"

#include <stdexcept>

namespace arti::renderer::vulkan {

VulkanImage::VulkanImage(const VulkanDevice& device, VulkanAllocator& allocator, const VulkanImageCreateInfo& info)
    : m_extent(info.extent),
      m_format(info.format)
{
    if (info.extent.width == 0 || info.extent.height == 0 || info.format == vk::Format::eUndefined || !info.usage) {
        throw std::invalid_argument("A Vulkan image requires extent, format, and usage.");
    }

    vk::ImageCreateInfo image_info{};
    image_info.setImageType(vk::ImageType::e2D)
        .setFormat(info.format)
        .setExtent({info.extent.width, info.extent.height, 1})
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(info.usage)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);
    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    m_image = allocator.createImage(image_info, allocation_info);

    vk::ImageSubresourceRange range{};
    range.setAspectMask(info.aspect).setBaseMipLevel(0).setLevelCount(1).setBaseArrayLayer(0).setLayerCount(1);
    vk::ImageViewCreateInfo view_info{};
    view_info.setImage(m_image.handle())
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(info.format)
        .setSubresourceRange(range);
    m_image_view = vk::raii::ImageView{device.device(), view_info};

    if (info.create_sampler) {
        vk::SamplerCreateInfo sampler_info{};
        sampler_info.setMagFilter(vk::Filter::eLinear)
            .setMinFilter(vk::Filter::eLinear)
            .setMipmapMode(vk::SamplerMipmapMode::eLinear)
            .setAddressModeU(vk::SamplerAddressMode::eRepeat)
            .setAddressModeV(vk::SamplerAddressMode::eRepeat)
            .setAddressModeW(vk::SamplerAddressMode::eRepeat)
            .setAnisotropyEnable(false)
            .setCompareEnable(false)
            .setMinLod(0.0f)
            .setMaxLod(0.0f)
            .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
            .setUnnormalizedCoordinates(false);
        m_sampler = vk::raii::Sampler{device.device(), sampler_info};
    }
}

vk::Image VulkanImage::image() const noexcept
{
    return m_image.handle();
}

const vk::raii::ImageView& VulkanImage::imageView() const noexcept
{
    return m_image_view;
}

const vk::raii::Sampler& VulkanImage::sampler() const noexcept
{
    return m_sampler;
}

vk::Extent2D VulkanImage::extent() const noexcept
{
    return m_extent;
}

vk::Format VulkanImage::format() const noexcept
{
    return m_format;
}

} // namespace arti::renderer::vulkan
