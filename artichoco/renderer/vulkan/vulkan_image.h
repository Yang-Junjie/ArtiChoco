#pragma once
#include "vulkan_allocator.h"
#include "vulkan_device.h"

#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

struct VulkanImageCreateInfo {
    vk::Extent2D extent{};
    vk::Format format{ vk::Format::eR8G8B8A8Unorm };
    vk::ImageUsageFlags usage{};
    vk::ImageAspectFlags aspect{ vk::ImageAspectFlagBits::eColor };
    uint32_t mip_levels{ 1 };
    uint32_t array_layers{ 1 };
    vk::ImageCreateFlags flags{};
    vk::ImageViewType view_type{ vk::ImageViewType::e2D };
};

class VulkanImage {
public:
    VulkanImage() = default;
    VulkanImage(const VulkanDevice& device, VulkanAllocator& allocator,
            const VulkanImageCreateInfo& info);

    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;
    VulkanImage(VulkanImage&&) noexcept = default;
    VulkanImage& operator=(VulkanImage&&) noexcept = default;

    vk::Image image() const noexcept;
    const vk::raii::ImageView& imageView() const noexcept;
    vk::raii::ImageView createView(const VulkanDevice& device, vk::ImageViewType view_type,
            vk::ImageSubresourceRange range) const;
    vk::raii::ImageView createLayerView(const VulkanDevice& device, uint32_t mip_level,
            uint32_t array_layer) const;
    vk::Extent2D extent() const noexcept;
    vk::Format format() const noexcept;
    uint32_t mipLevels() const noexcept;
    uint32_t arrayLayers() const noexcept;

private:
    AllocatedImage m_image;
    vk::raii::ImageView m_image_view{ nullptr };
    vk::Extent2D m_extent{};
    vk::Format m_format{ vk::Format::eUndefined };
    vk::ImageAspectFlags m_aspect{};
    vk::ImageCreateFlags m_flags{};
    uint32_t m_mip_levels{ 0 };
    uint32_t m_array_layers{ 0 };
};

} // namespace arti::renderer::vulkan
