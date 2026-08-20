#include "vulkan_resource_state.h"

namespace arti::renderer::vulkan {

vk::ImageSubresourceRange fullImageRange(vk::ImageAspectFlags aspect,
                                         uint32_t mip_levels,
                                         uint32_t array_layers) noexcept
{
    vk::ImageSubresourceRange range{};
    range.setAspectMask(aspect)
        .setBaseMipLevel(0)
        .setLevelCount(mip_levels)
        .setBaseArrayLayer(0)
        .setLayerCount(array_layers);
    return range;
}

VulkanImageState undefinedImageState() noexcept
{
    return {vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::eUndefined};
}

VulkanImageState colorAttachmentWriteState() noexcept
{
    return {vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eColorAttachmentOptimal};
}

VulkanImageState depthAttachmentReadWriteState() noexcept
{
    return {vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
            vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            vk::ImageLayout::eDepthAttachmentOptimal};
}

VulkanImageState computeStorageWriteState() noexcept
{
    return {vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderStorageWrite,
            vk::ImageLayout::eGeneral};
}

VulkanImageState transferReadState() noexcept
{
    return {vk::PipelineStageFlagBits2::eCopy,
            vk::AccessFlagBits2::eTransferRead,
            vk::ImageLayout::eTransferSrcOptimal};
}

VulkanImageState transferWriteState() noexcept
{
    return {vk::PipelineStageFlagBits2::eCopy,
            vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eTransferDstOptimal};
}

VulkanImageState fragmentSampledReadState() noexcept
{
    return {vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderSampledRead,
            vk::ImageLayout::eShaderReadOnlyOptimal};
}

VulkanImageState presentState() noexcept
{
    return {vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::ePresentSrcKHR};
}

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

vk::ImageMemoryBarrier2 makeImageBarrier(vk::Image image,
                                         vk::ImageAspectFlags aspect,
                                         VulkanImageState before,
                                         VulkanImageState after,
                                         uint32_t mip_levels,
                                         uint32_t array_layers) noexcept
{
    return makeImageBarrier(image, fullImageRange(aspect, mip_levels, array_layers), before, after);
}

} // namespace arti::renderer::vulkan
