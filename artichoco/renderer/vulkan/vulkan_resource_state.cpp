#include "vulkan_resource_state.h"

namespace arti::renderer::vulkan {

vk::BufferMemoryBarrier2 makeBufferBarrier(vk::Buffer buffer,
                                           vk::DeviceSize offset,
                                           vk::DeviceSize size,
                                           VulkanBufferState before,
                                           VulkanBufferState after) noexcept
{
    vk::BufferMemoryBarrier2 barrier{};
    barrier.setSrcStageMask(before.stages)
        .setSrcAccessMask(before.access)
        .setDstStageMask(after.stages)
        .setDstAccessMask(after.access)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setBuffer(buffer)
        .setOffset(offset)
        .setSize(size);
    return barrier;
}

vk::ImageMemoryBarrier2 makeImageBarrier(vk::Image image,
                                         vk::ImageSubresourceRange range,
                                         VulkanImageState before,
                                         VulkanImageState after) noexcept
{
    vk::ImageMemoryBarrier2 barrier{};
    barrier.setSrcStageMask(before.stages)
        .setSrcAccessMask(before.access)
        .setDstStageMask(after.stages)
        .setDstAccessMask(after.access)
        .setOldLayout(before.layout)
        .setNewLayout(after.layout)
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setImage(image)
        .setSubresourceRange(range);
    return barrier;
}

} // namespace arti::renderer::vulkan
