#include "artichoco/renderer/buffer_access.h"
#include "artichoco/renderer/texture_access.h"
#include "vulkan_allocator.h"
#include "vulkan_descriptor_allocator.h"
#include "vulkan_device.h"
#include "vulkan_frame_manager.h"
#include "vulkan_pass_context.h"
#include "vulkan_pipeline_cache.h"
#include "vulkan_upload_context.h"

#include <stdexcept>

namespace arti::renderer::vulkan {

VulkanPassPrepareContext::VulkanPassPrepareContext(const VulkanDevice& device,
                                                   VulkanAllocator& allocator,
                                                   VulkanUploadContext& upload_context,
                                                   VulkanDescriptorAllocator& descriptor_allocator,
                                                   VulkanPipelineCache& pipeline_cache,
                                                   size_t frame_slot_count) noexcept
    : m_device(device),
      m_allocator(allocator),
      m_upload_context(upload_context),
      m_descriptor_allocator(descriptor_allocator),
      m_pipeline_cache(pipeline_cache),
      m_frame_slot_count(frame_slot_count)
{}

const VulkanDevice& VulkanPassPrepareContext::device() const noexcept
{
    return m_device;
}

VulkanAllocator& VulkanPassPrepareContext::allocator() const noexcept
{
    return m_allocator;
}

VulkanUploadContext& VulkanPassPrepareContext::uploadContext() const noexcept
{
    return m_upload_context;
}

VulkanDescriptorAllocator& VulkanPassPrepareContext::descriptorAllocator() const noexcept
{
    return m_descriptor_allocator;
}

size_t VulkanPassPrepareContext::frameSlotCount() const noexcept
{
    return m_frame_slot_count;
}

VulkanPipelineCache& VulkanPassPrepareContext::pipelineCache() const noexcept
{
    return m_pipeline_cache;
}

VulkanPassContext::VulkanPassContext(VulkanFrameContext& frame,
                                     const detail::DeferredResourceOwner* resource_owner,
                                     VulkanPipelineCache& pipeline_cache) noexcept
    : m_frame(frame),
      m_resource_owner(resource_owner),
      m_pipeline_cache(pipeline_cache)
{}

VulkanFrameContext& VulkanPassContext::frame() const noexcept
{
    return m_frame;
}

VulkanCommandRecorder& VulkanPassContext::commands() const noexcept
{
    return m_frame.commands();
}

VulkanPipelineCache& VulkanPassContext::pipelineCache() const noexcept
{
    return m_pipeline_cache;
}

vk::Buffer VulkanPassContext::buffer(const VertexBuffer& vertex_buffer) const
{
    if (!detail::BufferAccess::isOwnedBy(vertex_buffer, m_resource_owner)) {
        throw std::invalid_argument("The vertex buffer belongs to another RenderDevice.");
    }
    return detail::BufferAccess::handle(vertex_buffer);
}

vk::Buffer VulkanPassContext::buffer(const IndexBuffer& index_buffer) const
{
    if (!detail::BufferAccess::isOwnedBy(index_buffer, m_resource_owner)) {
        throw std::invalid_argument("The index buffer belongs to another RenderDevice.");
    }
    return detail::BufferAccess::handle(index_buffer);
}

const VulkanImage& VulkanPassContext::image(const Texture2D& texture) const
{
    if (!detail::TextureAccess::isOwnedBy(texture, m_resource_owner)) {
        throw std::invalid_argument("The texture belongs to another RenderDevice.");
    }
    return detail::TextureAccess::image(texture);
}

} // namespace arti::renderer::vulkan
