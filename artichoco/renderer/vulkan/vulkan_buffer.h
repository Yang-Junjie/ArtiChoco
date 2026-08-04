#pragma once
#include "vulkan_allocator.h"
#include "vulkan_resource_state.h"

#include <cstddef>

#include <span>
#include <vulkan/vulkan.hpp>

namespace arti::renderer::vulkan {

class VulkanUploadContext;

enum class VulkanBufferMemory {
    DeviceLocal,
    HostVisible,
};

struct VulkanBufferCreateInfo {
    vk::DeviceSize size{0};
    vk::BufferUsageFlags usage{};
    VulkanBufferMemory memory{VulkanBufferMemory::DeviceLocal};
};

class VulkanBuffer {
public:
    VulkanBuffer() = default;
    VulkanBuffer(VulkanAllocator& allocator, const VulkanBufferCreateInfo& info);

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    VulkanBuffer(VulkanBuffer&&) noexcept = default;
    VulkanBuffer& operator=(VulkanBuffer&&) noexcept = default;

    void uploadInitial(VulkanUploadContext& upload_context,
                       std::span<const std::byte> data,
                       VulkanBufferState final_state,
                       vk::DeviceSize offset = 0);
    void write(std::span<const std::byte> data, vk::DeviceSize offset = 0);

    vk::Buffer buffer() const noexcept;
    vk::DeviceSize size() const noexcept;
    vk::BufferUsageFlags usage() const noexcept;
    VulkanBufferMemory memory() const noexcept;

private:
    void requireRange(std::span<const std::byte> data, vk::DeviceSize offset) const;

    AllocatedBuffer m_buffer;
    vk::DeviceSize m_size{0};
    vk::BufferUsageFlags m_usage{};
    VulkanBufferMemory m_memory{VulkanBufferMemory::DeviceLocal};
};

} // namespace arti::renderer::vulkan
