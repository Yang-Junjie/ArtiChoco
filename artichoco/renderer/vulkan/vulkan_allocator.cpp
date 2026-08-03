#define VMA_IMPLEMENTATION
#include "vulkan_allocator.h"

#include "artichoco/renderer/renderer_log.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace arti::renderer::vulkan {
namespace {

void checkVMAResult(VkResult result, const char* operation)
{
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string{operation} + " failed with " + vk::to_string(static_cast<vk::Result>(result)));
    }
}

} // namespace

AllocatedBuffer::AllocatedBuffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation) noexcept
    : m_allocator(allocator),
      m_buffer(buffer),
      m_allocation(allocation)
{}

AllocatedBuffer::~AllocatedBuffer()
{
    reset();
}

AllocatedBuffer::AllocatedBuffer(AllocatedBuffer&& other) noexcept
    : m_allocator(std::exchange(other.m_allocator, VK_NULL_HANDLE)),
      m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE)),
      m_allocation(std::exchange(other.m_allocation, VK_NULL_HANDLE)),
      m_mapped_data(std::exchange(other.m_mapped_data, nullptr))
{}

AllocatedBuffer& AllocatedBuffer::operator=(AllocatedBuffer&& other) noexcept
{
    if (this != &other) {
        reset();
        m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
        m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
        m_allocation = std::exchange(other.m_allocation, VK_NULL_HANDLE);
        m_mapped_data = std::exchange(other.m_mapped_data, nullptr);
    }
    return *this;
}

vk::Buffer AllocatedBuffer::handle() const noexcept
{
    return vk::Buffer{m_buffer};
}

VmaAllocation AllocatedBuffer::allocation() const noexcept
{
    return m_allocation;
}

void* AllocatedBuffer::map()
{
    if (m_mapped_data == nullptr) {
        checkVMAResult(vmaMapMemory(m_allocator, m_allocation, &m_mapped_data), "vmaMapMemory");
    }
    return m_mapped_data;
}

void AllocatedBuffer::unmap() noexcept
{
    if (m_mapped_data != nullptr) {
        vmaUnmapMemory(m_allocator, m_allocation);
        m_mapped_data = nullptr;
    }
}

void AllocatedBuffer::flush(vk::DeviceSize offset, vk::DeviceSize size) const
{
    checkVMAResult(vmaFlushAllocation(m_allocator, m_allocation, offset, size), "vmaFlushAllocation");
}

void AllocatedBuffer::reset() noexcept
{
    unmap();
    if (m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    }
    m_allocator = VK_NULL_HANDLE;
    m_buffer = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
}

AllocatedImage::AllocatedImage(VmaAllocator allocator, VkImage image, VmaAllocation allocation) noexcept
    : m_allocator(allocator),
      m_image(image),
      m_allocation(allocation)
{}

AllocatedImage::~AllocatedImage()
{
    reset();
}

AllocatedImage::AllocatedImage(AllocatedImage&& other) noexcept
    : m_allocator(std::exchange(other.m_allocator, VK_NULL_HANDLE)),
      m_image(std::exchange(other.m_image, VK_NULL_HANDLE)),
      m_allocation(std::exchange(other.m_allocation, VK_NULL_HANDLE))
{}

AllocatedImage& AllocatedImage::operator=(AllocatedImage&& other) noexcept
{
    if (this != &other) {
        reset();
        m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
        m_image = std::exchange(other.m_image, VK_NULL_HANDLE);
        m_allocation = std::exchange(other.m_allocation, VK_NULL_HANDLE);
    }
    return *this;
}

vk::Image AllocatedImage::handle() const noexcept
{
    return vk::Image{m_image};
}

VmaAllocation AllocatedImage::allocation() const noexcept
{
    return m_allocation;
}

void AllocatedImage::reset() noexcept
{
    if (m_image != VK_NULL_HANDLE) {
        vmaDestroyImage(m_allocator, m_image, m_allocation);
    }
    m_allocator = VK_NULL_HANDLE;
    m_image = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
}

VulkanAllocator::VulkanAllocator(const VulkanContext& context, const VulkanDevice& device)
{
    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.vulkanApiVersion = std::min(context.apiVersion(), device.physicalDevice().getProperties().apiVersion);
    allocator_info.instance = static_cast<VkInstance>(*context.instance());
    allocator_info.physicalDevice = static_cast<VkPhysicalDevice>(*device.physicalDevice());
    allocator_info.device = static_cast<VkDevice>(*device.device());
    checkVMAResult(vmaCreateAllocator(&allocator_info, &m_allocator), "vmaCreateAllocator");
    getLogChannel().info("Created Vulkan memory allocator");
}

VulkanAllocator::~VulkanAllocator()
{
    reset();
}

VulkanAllocator::VulkanAllocator(VulkanAllocator&& other) noexcept
    : m_allocator(std::exchange(other.m_allocator, VK_NULL_HANDLE))
{}

VulkanAllocator& VulkanAllocator::operator=(VulkanAllocator&& other) noexcept
{
    if (this != &other) {
        reset();
        m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
    }
    return *this;
}

AllocatedBuffer VulkanAllocator::createBuffer(
    const vk::BufferCreateInfo& buffer_info,
    const VmaAllocationCreateInfo& allocation_info) const
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    checkVMAResult(
        vmaCreateBuffer(
            m_allocator,
            reinterpret_cast<const VkBufferCreateInfo*>(&buffer_info),
            &allocation_info,
            &buffer,
            &allocation,
            nullptr),
        "vmaCreateBuffer");
    return AllocatedBuffer{m_allocator, buffer, allocation};
}

AllocatedImage VulkanAllocator::createImage(
    const vk::ImageCreateInfo& image_info,
    const VmaAllocationCreateInfo& allocation_info) const
{
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    checkVMAResult(
        vmaCreateImage(
            m_allocator,
            reinterpret_cast<const VkImageCreateInfo*>(&image_info),
            &allocation_info,
            &image,
            &allocation,
            nullptr),
        "vmaCreateImage");
    return AllocatedImage{m_allocator, image, allocation};
}

VmaAllocator VulkanAllocator::handle() const noexcept
{
    return m_allocator;
}

void VulkanAllocator::reset() noexcept
{
    if (m_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;
    }
}

} // namespace arti::renderer::vulkan
