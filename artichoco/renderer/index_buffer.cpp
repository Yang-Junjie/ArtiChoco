#include "index_buffer.h"

#include "buffer_access.h"
#include "vulkan/vulkan_allocator.h"
#include "vulkan/vulkan_resource_state.h"
#include "vulkan/vulkan_upload_context.h"

#include <stdexcept>
#include <utility>

namespace arti::renderer {

struct IndexBuffer::Impl {
    vulkan::AllocatedBuffer buffer;
    detail::DeferredResourceOwnerPtr owner;
    uint32_t index_count{0};
    IndexType index_type{IndexType::UInt32};

    ~Impl()
    {
        if (owner && buffer.handle()) {
            owner->deferRelease(std::packaged_task<void()>{
                [resource = std::move(buffer)]() mutable { (void)resource; },
            });
        }
    }
};

IndexBuffer::IndexBuffer(std::unique_ptr<Impl> impl) noexcept
    : m_impl(std::move(impl))
{}

IndexBuffer::~IndexBuffer() = default;
IndexBuffer::IndexBuffer(IndexBuffer&&) noexcept = default;
IndexBuffer& IndexBuffer::operator=(IndexBuffer&&) noexcept = default;

uint32_t IndexBuffer::indexCount() const noexcept
{
    return m_impl->index_count;
}

IndexType IndexBuffer::indexType() const noexcept
{
    return m_impl->index_type;
}

IndexBuffer detail::BufferAccess::createIndexBuffer(
    vulkan::VulkanAllocator& allocator,
    vulkan::VulkanUploadContext& upload_context,
    DeferredResourceOwnerPtr owner,
    std::span<const std::byte> data,
    uint32_t index_count,
    IndexType index_type)
{
    const size_t index_size = index_type == IndexType::UInt16 ? sizeof(uint16_t) : sizeof(uint32_t);
    if (!owner || index_count == 0 || data.empty() || data.size() != index_size * index_count) {
        throw std::invalid_argument("Index buffer data does not match its count and type.");
    }

    vk::BufferCreateInfo buffer_info{};
    buffer_info.setSize(data.size_bytes())
        .setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer)
        .setSharingMode(vk::SharingMode::eExclusive);
    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    auto impl = std::make_unique<IndexBuffer::Impl>();
    impl->buffer = allocator.createBuffer(buffer_info, allocation_info);
    impl->owner = owner;
    impl->index_count = index_count;
    impl->index_type = index_type;
    const vulkan::VulkanBufferState index_read{
        vk::PipelineStageFlagBits2::eIndexInput,
        vk::AccessFlagBits2::eIndexRead,
    };
    upload_context.uploadBuffer(data, impl->buffer.handle(), index_read);
    return IndexBuffer{std::move(impl)};
}

vk::Buffer detail::BufferAccess::handle(const IndexBuffer& buffer) noexcept
{
    return buffer.m_impl->buffer.handle();
}

bool detail::BufferAccess::isOwnedBy(const IndexBuffer& buffer, const DeferredResourceOwner* owner) noexcept
{
    return buffer.m_impl->owner.get() == owner;
}

} // namespace arti::renderer
