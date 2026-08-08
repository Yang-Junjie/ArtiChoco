#include "vertex_buffer.h"

#include "detail/buffer_access.h"
#include "vulkan/vulkan_allocator.h"
#include "vulkan/vulkan_resource_state.h"
#include "vulkan/vulkan_upload_context.h"

#include <cstring>
#include <stdexcept>
#include <utility>

namespace arti::renderer {
namespace {

uint32_t attributeSize(VertexAttributeType type)
{
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

void validateLayout(const VertexBufferLayout& layout)
{
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
    vulkan::AllocatedBuffer buffer;
    detail::DeferredResourceOwnerPtr owner;
    uint32_t vertex_count{0};
    VertexBufferLayout layout;

    ~Impl()
    {
        if (owner && buffer.handle()) {
            owner->deferRelease(std::packaged_task<void()>{
                [resource = std::move(buffer)]() mutable { (void)resource; },
            });
        }
    }
};

VertexBuffer::VertexBuffer(std::unique_ptr<Impl> impl) noexcept
    : m_impl(std::move(impl))
{}

VertexBuffer::~VertexBuffer() = default;
VertexBuffer::VertexBuffer(VertexBuffer&&) noexcept = default;
VertexBuffer& VertexBuffer::operator=(VertexBuffer&&) noexcept = default;

uint32_t VertexBuffer::vertexCount() const noexcept
{
    return m_impl->vertex_count;
}

const VertexBufferLayout& VertexBuffer::layout() const noexcept
{
    return m_impl->layout;
}

VertexBuffer detail::BufferAccess::createVertexBuffer(
    vulkan::VulkanAllocator& allocator,
    vulkan::VulkanUploadContext& upload_context,
    DeferredResourceOwnerPtr owner,
    std::span<const std::byte> data,
    uint32_t vertex_count,
    VertexBufferLayout layout)
{
    validateLayout(layout);
    if (!owner || vertex_count == 0 || data.empty() || data.size() != layout.stride * vertex_count) {
        throw std::invalid_argument("Vertex buffer data does not match its count and stride.");
    }

    vk::BufferCreateInfo buffer_info{};
    buffer_info.setSize(data.size_bytes())
        .setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer)
        .setSharingMode(vk::SharingMode::eExclusive);
    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    auto impl = std::make_unique<VertexBuffer::Impl>();
    impl->buffer = allocator.createBuffer(buffer_info, allocation_info);
    impl->owner = owner;
    impl->vertex_count = vertex_count;
    impl->layout = std::move(layout);
    const vulkan::VulkanBufferState vertex_read{
        vk::PipelineStageFlagBits2::eVertexAttributeInput,
        vk::AccessFlagBits2::eVertexAttributeRead,
    };
    upload_context.uploadBuffer(data, impl->buffer.handle(), vertex_read);
    return VertexBuffer{std::move(impl)};
}

vk::Buffer detail::BufferAccess::handle(const VertexBuffer& buffer) noexcept
{
    return buffer.m_impl->buffer.handle();
}

bool detail::BufferAccess::isOwnedBy(const VertexBuffer& buffer, const DeferredResourceOwner* owner) noexcept
{
    return buffer.m_impl->owner.get() == owner;
}

} // namespace arti::renderer
