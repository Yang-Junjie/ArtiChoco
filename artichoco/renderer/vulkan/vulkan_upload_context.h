#pragma once
#include "vulkan_allocator.h"
#include "vulkan_device.h"
#include "vulkan_resource_state.h"

#include <cstddef>

#include <span>
#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

class VulkanUploadContext {
public:
    VulkanUploadContext(const VulkanDevice& device, VulkanAllocator& allocator);

    VulkanUploadContext(const VulkanUploadContext&) = delete;
    VulkanUploadContext& operator=(const VulkanUploadContext&) = delete;

    void uploadBuffer(std::span<const std::byte> data,
                      vk::Buffer destination,
                      VulkanBufferState final_state,
                      vk::DeviceSize destination_offset = 0);
    void uploadImageRGBA8(std::span<const std::byte> data,
                          vk::Image destination,
                          vk::Extent2D extent,
                          VulkanImageState final_state);
    void uploadImage(std::span<const std::byte> data,
                     vk::Image destination,
                     std::span<const vk::BufferImageCopy> regions,
                     vk::ImageSubresourceRange destination_range,
                     VulkanImageState final_state);
    void uploadImageWithMipmaps(std::span<const std::byte> data,
                                vk::Image destination,
                                vk::Extent2D extent,
                                vk::Format format,
                                uint32_t bytes_per_texel,
                                VulkanImageState final_state);

private:
    const VulkanDevice& m_device;
    VulkanAllocator& m_allocator;
    vk::raii::CommandPool m_command_pool{nullptr};
    vk::raii::CommandBuffer m_command_buffer{nullptr};
    vk::raii::Fence m_fence{nullptr};

    AllocatedBuffer createStagingBuffer(std::span<const std::byte> data) const;
    void beginUpload();
    void submitAndWait();
};

} // namespace arti::renderer::vulkan
