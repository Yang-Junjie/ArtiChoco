#pragma once
#include "detail/deferred_resource_owner.h"
#include "index_buffer.h"
#include "vertex_buffer.h"

#include <vulkan/vulkan.hpp>

#include <cstddef>
#include <span>

namespace arti::renderer::vulkan {
class VulkanAllocator;
class VulkanUploadContext;
}

namespace arti::renderer::detail {

class BufferAccess {
public:
    static VertexBuffer createVertexBuffer(
        vulkan::VulkanAllocator& allocator,
        vulkan::VulkanUploadContext& upload_context,
        DeferredResourceOwnerPtr owner,
        std::span<const std::byte> data,
        uint32_t vertex_count,
        VertexBufferLayout layout);
    static IndexBuffer createIndexBuffer(
        vulkan::VulkanAllocator& allocator,
        vulkan::VulkanUploadContext& upload_context,
        DeferredResourceOwnerPtr owner,
        std::span<const std::byte> data,
        uint32_t index_count,
        IndexType index_type);

    static vk::Buffer handle(const VertexBuffer& buffer) noexcept;
    static vk::Buffer handle(const IndexBuffer& buffer) noexcept;
    static bool isOwnedBy(const VertexBuffer& buffer, const DeferredResourceOwner* owner) noexcept;
    static bool isOwnedBy(const IndexBuffer& buffer, const DeferredResourceOwner* owner) noexcept;
};

} // namespace arti::renderer::detail
