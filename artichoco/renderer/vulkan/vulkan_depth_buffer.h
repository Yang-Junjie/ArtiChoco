#pragma once
#include "vulkan_allocator.h"
#include "vulkan_device.h"

#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

class VulkanDepthBuffer {
public:
    VulkanDepthBuffer(const VulkanDevice& device, VulkanAllocator& allocator, vk::Extent2D extent);

    VulkanDepthBuffer(const VulkanDepthBuffer&) = delete;
    VulkanDepthBuffer& operator=(const VulkanDepthBuffer&) = delete;
    VulkanDepthBuffer(VulkanDepthBuffer&&) noexcept = default;
    VulkanDepthBuffer& operator=(VulkanDepthBuffer&&) noexcept = default;

    vk::Image image() const noexcept;
    const vk::raii::ImageView& imageView() const noexcept;
    vk::Format format() const noexcept;

private:
    AllocatedImage m_image;
    vk::raii::ImageView m_image_view{nullptr};
    vk::Format m_format{vk::Format::eUndefined};
};

} // namespace arti::renderer::vulkan
