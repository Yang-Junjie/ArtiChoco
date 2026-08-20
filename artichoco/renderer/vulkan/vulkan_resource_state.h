#pragma once

#include <vulkan/vulkan.hpp>

namespace arti::renderer::vulkan {

struct VulkanBufferState {
    vk::PipelineStageFlags2 stages{};
    vk::AccessFlags2 access{};
};

struct VulkanImageState {
    vk::PipelineStageFlags2 stages{};
    vk::AccessFlags2 access{};
    vk::ImageLayout layout{vk::ImageLayout::eUndefined};
};

vk::ImageSubresourceRange fullImageRange(vk::ImageAspectFlags aspect,
                                         uint32_t mip_levels = 1,
                                         uint32_t array_layers = 1) noexcept;

VulkanImageState undefinedImageState() noexcept;
VulkanImageState colorAttachmentWriteState() noexcept;
VulkanImageState depthAttachmentReadWriteState() noexcept;
VulkanImageState computeStorageWriteState() noexcept;
VulkanImageState transferReadState() noexcept;
VulkanImageState transferWriteState() noexcept;
VulkanImageState fragmentSampledReadState() noexcept;
VulkanImageState presentState() noexcept;

vk::BufferMemoryBarrier2 makeBufferBarrier(vk::Buffer buffer,
                                           vk::DeviceSize offset,
                                           vk::DeviceSize size,
                                           VulkanBufferState before,
                                           VulkanBufferState after) noexcept;

vk::ImageMemoryBarrier2 makeImageBarrier(vk::Image image,
                                         vk::ImageSubresourceRange range,
                                         VulkanImageState before,
                                         VulkanImageState after) noexcept;

vk::ImageMemoryBarrier2 makeImageBarrier(vk::Image image,
                                         vk::ImageAspectFlags aspect,
                                         VulkanImageState before,
                                         VulkanImageState after,
                                         uint32_t mip_levels = 1,
                                         uint32_t array_layers = 1) noexcept;

} // namespace arti::renderer::vulkan
