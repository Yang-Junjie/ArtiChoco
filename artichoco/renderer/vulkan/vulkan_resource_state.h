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

vk::BufferMemoryBarrier2 makeBufferBarrier(vk::Buffer buffer,
                                           vk::DeviceSize offset,
                                           vk::DeviceSize size,
                                           VulkanBufferState before,
                                           VulkanBufferState after) noexcept;

vk::ImageMemoryBarrier2 makeImageBarrier(vk::Image image,
                                         vk::ImageSubresourceRange range,
                                         VulkanImageState before,
                                         VulkanImageState after) noexcept;

} // namespace arti::renderer::vulkan
