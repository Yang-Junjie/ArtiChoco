#include "vulkan_command_recorder.h"
#include "vulkan_image.h"
#include "vulkan_upload_context.h"

#include <cstring>

#include <algorithm>
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
                                       VulkanBufferState final_state,
                                       vk::DeviceSize destination_offset)
{
    if (data.empty() || !destination) {
        throw std::invalid_argument("A buffer upload requires data and a destination.");
    }

    auto staging = createStagingBuffer(data);
    beginUpload();
    VulkanCommandRecorder commands{m_device, m_command_buffer};
    commands.handle().copyBuffer(
        staging.handle(), destination, vk::BufferCopy{0, destination_offset, data.size_bytes()});

    const VulkanBufferState transfer_write{
        vk::PipelineStageFlagBits2::eCopy,
        vk::AccessFlagBits2::eTransferWrite,
    };
    commands.bufferBarrier(
        makeBufferBarrier(destination, destination_offset, data.size_bytes(), transfer_write, final_state));
    commands.end();

    submitAndWait();
}

void VulkanUploadContext::uploadImageRGBA8(std::span<const std::byte> data,
                                           vk::Image destination,
                                           vk::Extent2D extent,
                                           VulkanImageState final_state)
{
    if (data.empty() || !destination || extent.width == 0 || extent.height == 0 ||
        data.size() != static_cast<size_t>(extent.width) * extent.height * 4) {
        throw std::invalid_argument("An RGBA image upload requires matching data and dimensions.");
    }

    vk::ImageSubresourceRange range{};
    range.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    vk::ImageSubresourceLayers layers{};
    layers.setAspectMask(vk::ImageAspectFlagBits::eColor).setMipLevel(0).setBaseArrayLayer(0).setLayerCount(1);
    vk::BufferImageCopy copy{};
    copy.setBufferOffset(0)
        .setBufferRowLength(0)
        .setBufferImageHeight(0)
        .setImageSubresource(layers)
        .setImageOffset({0, 0, 0})
        .setImageExtent({extent.width, extent.height, 1});
    uploadImage(data,
                destination,
                std::span<const vk::BufferImageCopy>{&copy, 1},
                range,
                final_state);
}

void VulkanUploadContext::uploadImage(std::span<const std::byte> data,
                                      vk::Image destination,
                                      std::span<const vk::BufferImageCopy> regions,
                                      vk::ImageSubresourceRange destination_range,
                                      VulkanImageState final_state)
{
    if (data.empty() || !destination || regions.empty() || !destination_range.aspectMask ||
        destination_range.levelCount == 0 || destination_range.layerCount == 0) {
        throw std::invalid_argument("An image upload requires data, copy regions, and a destination range.");
    }

    auto staging = createStagingBuffer(data);
    beginUpload();
    VulkanCommandRecorder commands{m_device, m_command_buffer};

    const VulkanImageState undefined{
        vk::PipelineStageFlagBits2::eNone,
        vk::AccessFlagBits2::eNone,
        vk::ImageLayout::eUndefined,
    };
    const VulkanImageState transfer_write{
        vk::PipelineStageFlagBits2::eCopy,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eTransferDstOptimal,
    };
    commands.imageBarrier(makeImageBarrier(destination, destination_range, undefined, transfer_write));
    commands.handle().copyBufferToImage(
        staging.handle(), destination, vk::ImageLayout::eTransferDstOptimal, regions);

    commands.imageBarrier(makeImageBarrier(destination, destination_range, transfer_write, final_state));
    commands.end();

    submitAndWait();
}

void VulkanUploadContext::uploadImageWithMipmaps(std::span<const std::byte> data,
                                                 vk::Image destination,
                                                 vk::Extent2D extent,
                                                 vk::Format format,
                                                 uint32_t bytes_per_texel,
                                                 VulkanImageState final_state)
{
    if (data.empty() || !destination || extent.width == 0 || extent.height == 0 ||
        bytes_per_texel == 0 ||
        data.size() != static_cast<size_t>(extent.width) * extent.height * bytes_per_texel) {
        throw std::invalid_argument(
            "A mipmapped image upload requires matching data, dimensions, and texel size.");
    }
    const auto format_properties = m_device.physicalDevice().getFormatProperties(format);
    const bool filterable = static_cast<bool>(
        format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear);
    if (!filterable) {
        throw std::invalid_argument(
            "Mipmap generation requires a texture format that supports optimal-tiling filtering.");
    }

    uint32_t mip_levels = imageMipLevelCount(extent);

    auto staging = createStagingBuffer(data);
    beginUpload();
    VulkanCommandRecorder commands{m_device, m_command_buffer};

    vk::ImageSubresourceRange all_range{};
    all_range.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(mip_levels)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    const VulkanImageState undefined{
        vk::PipelineStageFlagBits2::eNone,
        vk::AccessFlagBits2::eNone,
        vk::ImageLayout::eUndefined,
    };
    const VulkanImageState transfer_write{
        vk::PipelineStageFlagBits2::eCopy,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eTransferDstOptimal,
    };
    const VulkanImageState transfer_read{
        vk::PipelineStageFlagBits2::eCopy,
        vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eTransferSrcOptimal,
    };
    commands.imageBarrier(makeImageBarrier(destination, all_range, undefined, transfer_write));

    vk::ImageSubresourceLayers base_layers{};
    base_layers.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setMipLevel(0)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    vk::BufferImageCopy copy{};
    copy.setBufferOffset(0)
        .setBufferRowLength(0)
        .setBufferImageHeight(0)
        .setImageSubresource(base_layers)
        .setImageOffset({0, 0, 0})
        .setImageExtent({extent.width, extent.height, 1});
    commands.handle().copyBufferToImage(
        staging.handle(), destination, vk::ImageLayout::eTransferDstOptimal, copy);

    if (mip_levels > 1) {
        vk::ImageSubresourceRange base_range{};
        base_range.setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setBaseMipLevel(0)
            .setLevelCount(1)
            .setBaseArrayLayer(0)
            .setLayerCount(1);
        commands.imageBarrier(
            makeImageBarrier(destination, base_range, transfer_write, transfer_read));
        commands.generateMipmaps(destination, extent);
    }

    const VulkanImageState pre_final = mip_levels > 1 ? transfer_read : transfer_write;
    commands.imageBarrier(makeImageBarrier(destination, all_range, pre_final, final_state));
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
