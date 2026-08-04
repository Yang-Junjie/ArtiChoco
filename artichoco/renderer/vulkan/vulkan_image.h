#pragma once
#include "vulkan_allocator.h"
#include "vulkan_device.h"

#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

struct VulkanImageCreateInfo {
    vk::Extent2D extent{};
    vk::Format format{vk::Format::eR8G8B8A8Unorm};
    vk::ImageUsageFlags usage{};
    vk::ImageAspectFlags aspect{vk::ImageAspectFlagBits::eColor};
    bool create_sampler{false};
};

class VulkanImage {
public:
    VulkanImage() = default;
    VulkanImage(const VulkanDevice& device, VulkanAllocator& allocator, const VulkanImageCreateInfo& info);

    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;
    VulkanImage(VulkanImage&&) noexcept = default;
    VulkanImage& operator=(VulkanImage&&) noexcept = default;

    vk::Image image() const noexcept;
    const vk::raii::ImageView& imageView() const noexcept;
    const vk::raii::Sampler& sampler() const noexcept;
    vk::Extent2D extent() const noexcept;
    vk::Format format() const noexcept;

private:
    AllocatedImage m_image;
    vk::raii::ImageView m_image_view{nullptr};
    vk::raii::Sampler m_sampler{nullptr};
    vk::Extent2D m_extent{};
    vk::Format m_format{vk::Format::eUndefined};
};

} // namespace arti::renderer::vulkan
