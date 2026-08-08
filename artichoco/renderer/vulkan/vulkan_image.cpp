#include "vulkan_image.h"

#include <stdexcept>

namespace arti::renderer::vulkan {

VulkanImage::VulkanImage(const VulkanDevice& device, VulkanAllocator& allocator,
        const VulkanImageCreateInfo& info)
        : m_extent(info.extent),
          m_format(info.format),
          m_aspect(info.aspect),
          m_flags(info.flags),
          m_mip_levels(info.mip_levels),
          m_array_layers(info.array_layers) {
    if (info.extent.width == 0 || info.extent.height == 0 ||
            info.format == vk::Format::eUndefined || !info.usage || !info.aspect ||
            info.mip_levels == 0 || info.array_layers == 0) {
        throw std::invalid_argument("A Vulkan image requires extent, format, and usage.");
    }

    const bool cube_compatible =
            static_cast<bool>(info.flags & vk::ImageCreateFlagBits::eCubeCompatible);
    if (cube_compatible && (info.extent.width != info.extent.height || info.array_layers < 6)) {
        throw std::invalid_argument("A cube-compatible image requires square dimensions and at "
                                    "least six array layers.");
    }
    if ((info.view_type == vk::ImageViewType::eCube ||
                info.view_type == vk::ImageViewType::eCubeArray) &&
            (!cube_compatible || info.extent.width != info.extent.height || info.array_layers < 6 ||
                    info.array_layers % 6 != 0)) {
        throw std::invalid_argument("A cube image view requires square, cube-compatible images "
                                    "with layers in groups of six.");
    }
    if (info.view_type == vk::ImageViewType::eCube && info.array_layers != 6) {
        throw std::invalid_argument("A cube image view requires exactly six array layers.");
    }
    if (info.view_type == vk::ImageViewType::e2D && info.array_layers != 1) {
        throw std::invalid_argument("A 2D image view requires exactly one array layer.");
    }

    vk::ImageCreateInfo image_info{};
    image_info.setFlags(info.flags)
            .setImageType(vk::ImageType::e2D)
            .setFormat(info.format)
            .setExtent({ info.extent.width, info.extent.height, 1 })
            .setMipLevels(info.mip_levels)
            .setArrayLayers(info.array_layers)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(info.usage)
            .setSharingMode(vk::SharingMode::eExclusive)
            .setInitialLayout(vk::ImageLayout::eUndefined);
    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    m_image = allocator.createImage(image_info, allocation_info);

    vk::ImageSubresourceRange range{};
    range.setAspectMask(info.aspect)
            .setBaseMipLevel(0)
            .setLevelCount(info.mip_levels)
            .setBaseArrayLayer(0)
            .setLayerCount(info.array_layers);
    m_image_view = createView(device, info.view_type, range);
}

vk::raii::ImageView VulkanImage::createView(const VulkanDevice& device, vk::ImageViewType view_type,
        vk::ImageSubresourceRange range) const {
    if (!m_image.handle() || !range.aspectMask || range.levelCount == 0 || range.layerCount == 0 ||
            range.baseMipLevel >= m_mip_levels ||
            range.levelCount > m_mip_levels - range.baseMipLevel ||
            range.baseArrayLayer >= m_array_layers ||
            range.layerCount > m_array_layers - range.baseArrayLayer) {
        throw std::invalid_argument(
                "The Vulkan image view range is outside the image subresources.");
    }

    switch (view_type) {
        case vk::ImageViewType::e2D:
            if (range.layerCount != 1) {
                throw std::invalid_argument("A 2D image view requires exactly one array layer.");
            }
            break;
        case vk::ImageViewType::e2DArray:
            break;
        case vk::ImageViewType::eCube:
            if (!(m_flags & vk::ImageCreateFlagBits::eCubeCompatible) || range.layerCount != 6 ||
                    range.baseArrayLayer % 6 != 0) {
                throw std::invalid_argument("A cube image view requires six aligned layers of a "
                                            "cube-compatible image.");
            }
            break;
        case vk::ImageViewType::eCubeArray:
            if (!(m_flags & vk::ImageCreateFlagBits::eCubeCompatible) || range.layerCount < 6 ||
                    range.layerCount % 6 != 0 || range.baseArrayLayer % 6 != 0) {
                throw std::invalid_argument(
                        "A cube-array image view requires aligned layer groups of six.");
            }
            break;
        default:
            throw std::invalid_argument(
                    "A VulkanImage only supports 2D and cube-compatible views.");
    }

    vk::ImageViewCreateInfo view_info{};
    view_info.setImage(m_image.handle())
            .setViewType(view_type)
            .setFormat(m_format)
            .setSubresourceRange(range);
    return vk::raii::ImageView{ device.device(), view_info };
}

vk::raii::ImageView VulkanImage::createLayerView(const VulkanDevice& device, uint32_t mip_level,
        uint32_t array_layer) const {
    vk::ImageSubresourceRange range;
    range.setAspectMask(m_aspect)
            .setBaseMipLevel(mip_level)
            .setLevelCount(1)
            .setBaseArrayLayer(array_layer)
            .setLayerCount(1);
    return createView(device, vk::ImageViewType::e2D, range);
}

vk::Image VulkanImage::image() const noexcept { return m_image.handle(); }

const vk::raii::ImageView& VulkanImage::imageView() const noexcept { return m_image_view; }

vk::Extent2D VulkanImage::extent() const noexcept { return m_extent; }

vk::Format VulkanImage::format() const noexcept { return m_format; }

uint32_t VulkanImage::mipLevels() const noexcept { return m_mip_levels; }

uint32_t VulkanImage::arrayLayers() const noexcept { return m_array_layers; }

} // namespace arti::renderer::vulkan
