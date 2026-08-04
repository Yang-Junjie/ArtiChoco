#pragma once
#include <cstddef>

#include <vulkan/vulkan.hpp>

namespace arti::renderer {
class IndexBuffer;
class Texture2D;
class VertexBuffer;

namespace detail {
class DeferredResourceOwner;
}

namespace vulkan {

class VulkanAllocator;
class VulkanCommandRecorder;
class VulkanDescriptorAllocator;
class VulkanDevice;
class VulkanFrameContext;
class VulkanImage;

class VulkanPassPrepareContext {
public:
    VulkanPassPrepareContext(const VulkanDevice& device,
                             VulkanAllocator& allocator,
                             VulkanDescriptorAllocator& descriptor_allocator,
                             size_t frame_count) noexcept;

    VulkanPassPrepareContext(const VulkanPassPrepareContext&) = delete;
    VulkanPassPrepareContext& operator=(const VulkanPassPrepareContext&) = delete;

    const VulkanDevice& device() const noexcept;
    VulkanAllocator& allocator() const noexcept;
    VulkanDescriptorAllocator& descriptorAllocator() const noexcept;
    size_t frameCount() const noexcept;

private:
    const VulkanDevice& m_device;
    VulkanAllocator& m_allocator;
    VulkanDescriptorAllocator& m_descriptor_allocator;
    size_t m_frame_count{0};
};

class VulkanPassContext {
public:
    VulkanPassContext(VulkanFrameContext& frame, const detail::DeferredResourceOwner* resource_owner) noexcept;

    VulkanPassContext(const VulkanPassContext&) = delete;
    VulkanPassContext& operator=(const VulkanPassContext&) = delete;

    VulkanFrameContext& frame() const noexcept;
    VulkanCommandRecorder& commands() const noexcept;
    vk::Buffer buffer(const VertexBuffer& vertex_buffer) const;
    vk::Buffer buffer(const IndexBuffer& index_buffer) const;
    const VulkanImage& image(const Texture2D& texture) const;

private:
    VulkanFrameContext& m_frame;
    const detail::DeferredResourceOwner* m_resource_owner{nullptr};
};

} // namespace vulkan
} // namespace arti::renderer
