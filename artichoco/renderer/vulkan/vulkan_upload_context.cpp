#include "vulkan_command_recorder.h"
#include "vulkan_upload_context.h"

#include <cstring>

#include <array>
#include <limits>
#include <stdexcept>

namespace arti::renderer::vulkan {

VulkanUploadContext::VulkanUploadContext(const VulkanDevice& device, VulkanAllocator& allocator)
    : m_device(device),
      m_allocator(allocator)
{
    vk::CommandPoolCreateInfo pool_info{};
    pool_info.setFlags(vk::CommandPoolCreateFlagBits::eTransient).setQueueFamilyIndex(device.graphicsQueueFamily());
    m_command_pool = vk::raii::CommandPool{device.device(), pool_info};

    vk::CommandBufferAllocateInfo allocate_info{};
    allocate_info.setCommandPool(*m_command_pool).setLevel(vk::CommandBufferLevel::ePrimary).setCommandBufferCount(1);
    vk::raii::CommandBuffers command_buffers{device.device(), allocate_info};
    m_command_buffer = std::move(command_buffers.front());
    m_fence = vk::raii::Fence{device.device(), vk::FenceCreateInfo{}};
}

void VulkanUploadContext::uploadBuffer(std::span<const std::byte> data,
                                       vk::Buffer destination,
                                       vk::AccessFlags2 destination_access)
{
    if (data.empty() || !destination) {
        throw std::invalid_argument("A buffer upload requires data and a destination.");
    }

    auto staging = createStagingBuffer(data);
    beginUpload();
    VulkanCommandRecorder commands{m_device, m_command_buffer};
    commands.handle().copyBuffer(staging.handle(), destination, vk::BufferCopy{0, 0, data.size_bytes()});

    vk::BufferMemoryBarrier2 barrier{};
    barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eCopy)
        .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eVertexInput)
        .setDstAccessMask(destination_access)
        .setBuffer(destination)
        .setOffset(0)
        .setSize(data.size_bytes());
    commands.bufferBarrier(barrier);
    commands.end();

    submitAndWait();
}

void VulkanUploadContext::uploadImageRGBA8(std::span<const std::byte> data, vk::Image destination, vk::Extent2D extent)
{
    if (data.empty() || !destination || extent.width == 0 || extent.height == 0 ||
        data.size() != static_cast<size_t>(extent.width) * extent.height * 4) {
        throw std::invalid_argument("An RGBA image upload requires matching data and dimensions.");
    }

    auto staging = createStagingBuffer(data);
    beginUpload();
    VulkanCommandRecorder commands{m_device, m_command_buffer};

    vk::ImageSubresourceRange range{};
    range.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    vk::ImageMemoryBarrier2 to_transfer{};
    to_transfer.setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
        .setSrcAccessMask(vk::AccessFlagBits2::eNone)
        .setDstStageMask(vk::PipelineStageFlagBits2::eCopy)
        .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setOldLayout(vk::ImageLayout::eUndefined)
        .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
        .setImage(destination)
        .setSubresourceRange(range);
    commands.imageBarrier(to_transfer);

    vk::ImageSubresourceLayers layers{};
    layers.setAspectMask(vk::ImageAspectFlagBits::eColor).setMipLevel(0).setBaseArrayLayer(0).setLayerCount(1);
    vk::BufferImageCopy copy{};
    copy.setBufferOffset(0)
        .setBufferRowLength(0)
        .setBufferImageHeight(0)
        .setImageSubresource(layers)
        .setImageOffset({0, 0, 0})
        .setImageExtent({extent.width, extent.height, 1});
    commands.handle().copyBufferToImage(staging.handle(), destination, vk::ImageLayout::eTransferDstOptimal, copy);

    vk::ImageMemoryBarrier2 to_shader_read{};
    to_shader_read.setSrcStageMask(vk::PipelineStageFlagBits2::eCopy)
        .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
        .setDstAccessMask(vk::AccessFlagBits2::eShaderSampledRead)
        .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
        .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setImage(destination)
        .setSubresourceRange(range);
    commands.imageBarrier(to_shader_read);
    commands.end();

    submitAndWait();
}

AllocatedBuffer VulkanUploadContext::createStagingBuffer(std::span<const std::byte> data) const
{
    vk::BufferCreateInfo staging_info{};
    staging_info.setSize(data.size_bytes())
        .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
        .setSharingMode(vk::SharingMode::eExclusive);
    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    auto staging = m_allocator.createBuffer(staging_info, allocation_info);
    std::memcpy(staging.map(), data.data(), data.size_bytes());
    staging.flush();
    staging.unmap();
    return staging;
}

void VulkanUploadContext::beginUpload()
{
    const std::array fences = {*m_fence};
    m_device.device().resetFences(fences);
    m_command_pool.reset();
    VulkanCommandRecorder{m_device, m_command_buffer}.begin();
}

void VulkanUploadContext::submitAndWait()
{
    const std::array fences = {*m_fence};

    vk::CommandBufferSubmitInfo command_buffer_info{};
    command_buffer_info.setCommandBuffer(*m_command_buffer);
    vk::SubmitInfo2 submit_info{};
    submit_info.setCommandBufferInfos(command_buffer_info);
    const std::array submits = {submit_info};
    if (m_device.usesCore13()) {
        m_device.graphicsQueue().submit2(submits, *m_fence);
    } else {
        m_device.graphicsQueue().submit2KHR(submits, *m_fence);
    }
    if (m_device.device().waitForFences(fences, true, std::numeric_limits<uint64_t>::max()) != vk::Result::eSuccess) {
        throw std::runtime_error("Timed out while uploading a Vulkan resource.");
    }
}

} // namespace arti::renderer::vulkan
