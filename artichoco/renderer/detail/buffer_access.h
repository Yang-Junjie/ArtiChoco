#pragma once
#include "artichoco/renderer/detail/resource_owner.h"
#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/vertex_buffer.h"

#include <cstddef>
#include <span>
#include <string_view>

namespace nvrhi {
class IBuffer;
class IDevice;
} // namespace nvrhi

namespace arti::renderer::detail {

class BufferAccess {
public:
    static VertexBuffer createVertexBuffer(nvrhi::IDevice& device, ResourceOwnerPtr owner,
            std::span<const std::byte> data, uint32_t vertex_count, VertexBufferLayout layout,
            std::string_view debug_name);
    static IndexBuffer createIndexBuffer(nvrhi::IDevice& device, ResourceOwnerPtr owner,
            std::span<const std::byte> data, uint32_t index_count, IndexType index_type,
            std::string_view debug_name);

    static nvrhi::IBuffer& nvrhiHandle(const VertexBuffer& buffer) noexcept;
    static nvrhi::IBuffer& nvrhiHandle(const IndexBuffer& buffer) noexcept;
    static bool isOwnedBy(const VertexBuffer& buffer, const ResourceOwner* owner) noexcept;
    static bool isOwnedBy(const IndexBuffer& buffer, const ResourceOwner* owner) noexcept;
};

} // namespace arti::renderer::detail
