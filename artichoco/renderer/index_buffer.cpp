#include "index_buffer.h"

#include "detail/buffer_access.h"
#include "vulkan/nvrhi_resource_upload.h"

#include <nvrhi/nvrhi.h>

#include <stdexcept>
#include <utility>

namespace arti::renderer {

struct IndexBuffer::Impl {
    detail::ResourceOwnerPtr owner;
    nvrhi::BufferHandle buffer;
    uint32_t index_count{ 0 };
    IndexType index_type{ IndexType::UInt32 };
};

IndexBuffer::IndexBuffer(std::unique_ptr<Impl> impl) noexcept
        : m_impl(std::move(impl)) {}

IndexBuffer::~IndexBuffer() = default;
IndexBuffer::IndexBuffer(IndexBuffer&&) noexcept = default;
IndexBuffer& IndexBuffer::operator=(IndexBuffer&&) noexcept = default;

uint32_t IndexBuffer::indexCount() const noexcept { return m_impl->index_count; }

IndexType IndexBuffer::indexType() const noexcept { return m_impl->index_type; }

IndexBuffer detail::BufferAccess::createIndexBuffer(nvrhi::IDevice& device,
        ResourceOwnerPtr owner,
        std::span<const std::byte> data, uint32_t index_count, IndexType index_type) {
    const size_t index_size = index_type == IndexType::UInt16 ? sizeof(uint16_t) : sizeof(uint32_t);
    if (!owner || index_count == 0 || data.empty() || data.size() != index_size * index_count) {
        throw std::invalid_argument("Index buffer data does not match its count and type.");
    }

    auto impl = std::make_unique<IndexBuffer::Impl>();
    nvrhi::BufferDesc buffer_desc;
    buffer_desc.setByteSize(data.size_bytes())
            .setDebugName("ArtiChoco IndexBuffer")
            .setIsIndexBuffer(true);
    impl->buffer = vulkan::createAndUploadNvrhiBuffer(
            device, std::move(buffer_desc), data, nvrhi::ResourceStates::IndexBuffer);
    impl->owner = owner;
    impl->index_count = index_count;
    impl->index_type = index_type;

    return IndexBuffer{ std::move(impl) };
}

nvrhi::IBuffer& detail::BufferAccess::nvrhiHandle(const IndexBuffer& buffer) noexcept {
    return *buffer.m_impl->buffer;
}

bool detail::BufferAccess::isOwnedBy(const IndexBuffer& buffer,
        const ResourceOwner* owner) noexcept {
    return buffer.m_impl->owner.get() == owner;
}

} // namespace arti::renderer
