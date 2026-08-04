#include "vulkan_frame_slot.h"

#include <utility>

namespace arti::renderer::vulkan {

VulkanFrameSlot::VulkanFrameSlot(const VulkanDevice& device)
{
    vk::CommandPoolCreateInfo pool_info{};
    pool_info.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        .setQueueFamilyIndex(device.graphicsQueueFamily());
    m_command_pool = vk::raii::CommandPool{device.device(), pool_info};

    vk::CommandBufferAllocateInfo allocate_info{};
    allocate_info.setCommandPool(*m_command_pool).setLevel(vk::CommandBufferLevel::ePrimary).setCommandBufferCount(1);
    vk::raii::CommandBuffers command_buffers{device.device(), allocate_info};
    m_command_buffer = std::move(command_buffers.front());

    m_image_available = vk::raii::Semaphore{device.device(), vk::SemaphoreCreateInfo{}};

    vk::FenceCreateInfo fence_info{};
    fence_info.setFlags(vk::FenceCreateFlagBits::eSignaled);
    m_in_flight = vk::raii::Fence{device.device(), fence_info};
}

const vk::raii::CommandPool& VulkanFrameSlot::commandPool() const noexcept
{
    return m_command_pool;
}

const vk::raii::CommandBuffer& VulkanFrameSlot::commandBuffer() const noexcept
{
    return m_command_buffer;
}

const vk::raii::Semaphore& VulkanFrameSlot::imageAvailableSemaphore() const noexcept
{
    return m_image_available;
}

const vk::raii::Fence& VulkanFrameSlot::inFlightFence() const noexcept
{
    return m_in_flight;
}

} // namespace arti::renderer::vulkan
