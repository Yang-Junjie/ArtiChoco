#pragma once
#include "vulkan_allocator.h"
#include "vulkan_device.h"

#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <span>

namespace arti::renderer::vulkan {

class VulkanUploadContext {
public:
    VulkanUploadContext(const VulkanDevice& device, VulkanAllocator& allocator);

    VulkanUploadContext(const VulkanUploadContext&) = delete;
    VulkanUploadContext& operator=(const VulkanUploadContext&) = delete;

    void uploadBuffer(std::span<const std::byte> data, vk::Buffer destination, vk::AccessFlags2 destination_access);
    void uploadImageRGBA8(std::span<const std::byte> data, vk::Image destination, vk::Extent2D extent);

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
