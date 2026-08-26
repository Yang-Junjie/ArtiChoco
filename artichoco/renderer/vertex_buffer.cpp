#include "vertex_buffer.h"

#include "detail/buffer_access.h"
#include "vulkan/nvrhi_resource_upload.h"

#include <nvrhi/nvrhi.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace arti::renderer {
namespace {

uint32_t attributeSize(VertexAttributeType type) {
    switch (type) {
        case VertexAttributeType::Float2:
            return sizeof(float) * 2;
        case VertexAttributeType::Float3:
            return sizeof(float) * 3;
        case VertexAttributeType::Float4:
            return sizeof(float) * 4;
    }
    throw std::invalid_argument("Unsupported vertex attribute type.");
}

void validateLayout(const VertexBufferLayout& layout) {
    if (layout.stride == 0 || layout.attributes.empty()) {
        throw std::invalid_argument("A vertex buffer requires a non-empty layout.");
    }
    for (size_t index = 0; index < layout.attributes.size(); ++index) {
        const auto& attribute = layout.attributes[index];
        if (attribute.offset + attributeSize(attribute.type) > layout.stride) {
            throw std::invalid_argument("A vertex attribute exceeds the vertex stride.");
        }
        for (size_t previous = 0; previous < index; ++previous) {
            if (layout.attributes[previous].location == attribute.location) {
                throw std::invalid_argument("Vertex attribute locations must be unique.");
            }
        }
    }
}

} // namespace

struct VertexBuffer::Impl {
    detail::ResourceOwnerPtr owner;
    nvrhi::BufferHandle buffer;
    uint32_t vertex_count{ 0 };
    VertexBufferLayout layout;
};

VertexBuffer::VertexBuffer(std::unique_ptr<Impl> impl) noexcept
        : m_impl(std::move(impl)) {}

VertexBuffer::~VertexBuffer() = default;
VertexBuffer::VertexBuffer(VertexBuffer&&) noexcept = default;
VertexBuffer& VertexBuffer::operator=(VertexBuffer&&) noexcept = default;

uint32_t VertexBuffer::vertexCount() const noexcept { return m_impl->vertex_count; }

const VertexBufferLayout& VertexBuffer::layout() const noexcept { return m_impl->layout; }

VertexBuffer detail::BufferAccess::createVertexBuffer(nvrhi::IDevice& device,
        ResourceOwnerPtr owner, std::span<const std::byte> data, uint32_t vertex_count,
        VertexBufferLayout layout, std::string_view debug_name) {
    validateLayout(layout);
    if (!owner || vertex_count == 0 || data.empty() ||
            data.size() != layout.stride * vertex_count) {
        throw std::invalid_argument("Vertex buffer data does not match its count and stride.");
    }

    auto impl = std::make_unique<VertexBuffer::Impl>();
    nvrhi::BufferDesc buffer_desc;
    buffer_desc.setByteSize(data.size_bytes())
            .setDebugName(debug_name.empty() ? "ArtiChoco VertexBuffer" : std::string{ debug_name })
            .setIsVertexBuffer(true);
    impl->buffer = vulkan::createAndUploadNvrhiBuffer(device, std::move(buffer_desc), data,
            nvrhi::ResourceStates::VertexBuffer);
    impl->owner = owner;
    impl->vertex_count = vertex_count;
    impl->layout = std::move(layout);
    return VertexBuffer{ std::move(impl) };
}

nvrhi::IBuffer& detail::BufferAccess::nvrhiHandle(const VertexBuffer& buffer) noexcept {
    return *buffer.m_impl->buffer;
}

bool detail::BufferAccess::isOwnedBy(const VertexBuffer& buffer,
        const ResourceOwner* owner) noexcept {
    return buffer.m_impl->owner.get() == owner;
}

} // namespace arti::renderer
