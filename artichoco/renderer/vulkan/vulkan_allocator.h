#pragma once
#include "vulkan_context.h"
#include "vulkan_device.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace arti::renderer::vulkan {

class AllocatedBuffer {
public:
    AllocatedBuffer() = default;
    ~AllocatedBuffer();

    AllocatedBuffer(const AllocatedBuffer&) = delete;
    AllocatedBuffer& operator=(const AllocatedBuffer&) = delete;
    AllocatedBuffer(AllocatedBuffer&& other) noexcept;
    AllocatedBuffer& operator=(AllocatedBuffer&& other) noexcept;

    vk::Buffer handle() const noexcept;
    VmaAllocation allocation() const noexcept;
    void* map();
    void unmap() noexcept;
    void flush(vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE) const;

private:
    friend class VulkanAllocator;
    AllocatedBuffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation) noexcept;
    void reset() noexcept;

    VmaAllocator m_allocator{VK_NULL_HANDLE};
    VkBuffer m_buffer{VK_NULL_HANDLE};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
    void* m_mapped_data{nullptr};
};

class AllocatedImage {
public:
    AllocatedImage() = default;
    ~AllocatedImage();

    AllocatedImage(const AllocatedImage&) = delete;
    AllocatedImage& operator=(const AllocatedImage&) = delete;
    AllocatedImage(AllocatedImage&& other) noexcept;
    AllocatedImage& operator=(AllocatedImage&& other) noexcept;

    vk::Image handle() const noexcept;
    VmaAllocation allocation() const noexcept;

private:
    friend class VulkanAllocator;
    AllocatedImage(VmaAllocator allocator, VkImage image, VmaAllocation allocation) noexcept;
    void reset() noexcept;

    VmaAllocator m_allocator{VK_NULL_HANDLE};
    VkImage m_image{VK_NULL_HANDLE};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
};

class VulkanAllocator {
public:
    VulkanAllocator(const VulkanContext& context, const VulkanDevice& device);
    ~VulkanAllocator();

    VulkanAllocator(const VulkanAllocator&) = delete;
    VulkanAllocator& operator=(const VulkanAllocator&) = delete;
    VulkanAllocator(VulkanAllocator&& other) noexcept;
    VulkanAllocator& operator=(VulkanAllocator&& other) noexcept;

    AllocatedBuffer createBuffer(
        const vk::BufferCreateInfo& buffer_info,
        const VmaAllocationCreateInfo& allocation_info) const;

    AllocatedImage createImage(
        const vk::ImageCreateInfo& image_info,
        const VmaAllocationCreateInfo& allocation_info) const;
        
    VmaAllocator handle() const noexcept;

private:
    void reset() noexcept;

    VmaAllocator m_allocator{VK_NULL_HANDLE};
};

} // namespace arti::renderer::vulkan
