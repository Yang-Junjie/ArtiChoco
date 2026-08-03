#pragma once
#include "vulkan_device.h"

#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

class FrameResources {
public:
    explicit FrameResources(const VulkanDevice& device);

    FrameResources(const FrameResources&) = delete;
    FrameResources& operator=(const FrameResources&) = delete;
    FrameResources(FrameResources&&) noexcept = default;
    FrameResources& operator=(FrameResources&&) noexcept = default;

    const vk::raii::CommandPool& commandPool() const noexcept;
    const vk::raii::CommandBuffer& commandBuffer() const noexcept;
    const vk::raii::Semaphore& imageAvailableSemaphore() const noexcept;
    const vk::raii::Fence& inFlightFence() const noexcept;

private:
    vk::raii::CommandPool m_command_pool{nullptr};
    vk::raii::CommandBuffer m_command_buffer{nullptr};
    vk::raii::Semaphore m_image_available{nullptr};
    vk::raii::Fence m_in_flight{nullptr};
};

} // namespace arti::renderer::vulkan
