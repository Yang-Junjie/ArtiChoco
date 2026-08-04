#include "vulkan_buffer.h"

#include "vulkan_upload_context.h"

#include <cstring>
#include <stdexcept>

namespace arti::renderer::vulkan {

VulkanBuffer::VulkanBuffer(VulkanAllocator& allocator, const VulkanBufferCreateInfo& info)
    : m_size(info.size),
      m_memory(info.memory)
{
    if (info.size == 0 || !info.usage) {
        throw std::invalid_argument("A Vulkan buffer requires a non-zero size and usage.");
    }

    vk::BufferCreateInfo buffer_info{};
    m_usage = info.usage;
    if (info.memory == VulkanBufferMemory::DeviceLocal) {
        m_usage |= vk::BufferUsageFlagBits::eTransferDst;
    }
    buffer_info.setSize(info.size).setUsage(m_usage).setSharingMode(vk::SharingMode::eExclusive);

    VmaAllocationCreateInfo allocation_info{};
    if (info.memory == VulkanBufferMemory::DeviceLocal) {
        allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    } else {
        allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    m_buffer = allocator.createBuffer(buffer_info, allocation_info);
    if (info.memory == VulkanBufferMemory::HostVisible) {
        (void)m_buffer.map();
    }
}

void VulkanBuffer::uploadInitial(VulkanUploadContext& upload_context,
                                 std::span<const std::byte> data,
                                 VulkanBufferState final_state,
                                 vk::DeviceSize offset)
{
    if (m_memory != VulkanBufferMemory::DeviceLocal) {
        throw std::logic_error("Only device-local Vulkan buffers use staging uploads.");
    }
    requireRange(data, offset);
    upload_context.uploadBuffer(data, m_buffer.handle(), final_state, offset);
}

void VulkanBuffer::write(std::span<const std::byte> data, vk::DeviceSize offset)
{
    if (m_memory != VulkanBufferMemory::HostVisible) {
        throw std::logic_error("Only host-visible Vulkan buffers can be written directly.");
    }
    requireRange(data, offset);
    auto* destination = static_cast<std::byte*>(m_buffer.map()) + offset;
    std::memcpy(destination, data.data(), data.size_bytes());
    m_buffer.flush(offset, data.size_bytes());
}

vk::Buffer VulkanBuffer::buffer() const noexcept
{
    return m_buffer.handle();
}

vk::DeviceSize VulkanBuffer::size() const noexcept
{
    return m_size;
}

vk::BufferUsageFlags VulkanBuffer::usage() const noexcept
{
    return m_usage;
}

VulkanBufferMemory VulkanBuffer::memory() const noexcept
{
    return m_memory;
}

void VulkanBuffer::requireRange(std::span<const std::byte> data, vk::DeviceSize offset) const
{
    if (data.empty() || offset > m_size || data.size_bytes() > m_size - offset) {
        throw std::out_of_range("Vulkan buffer data exceeds the allocated range.");
    }
}

} // namespace arti::renderer::vulkan
