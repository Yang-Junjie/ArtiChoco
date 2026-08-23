#include "render_pass.h"

#include "detail/buffer_access.h"
#include "detail/texture_access.h"

#include <stdexcept>

namespace arti::renderer {

RenderPassPrepareContext::RenderPassPrepareContext(nvrhi::IDevice& device,
        nvrhi::IFramebuffer& framebuffer, size_t frame_slot_count) noexcept
    : m_device(device),
      m_framebuffer(framebuffer),
      m_frame_slot_count(frame_slot_count)
{}

nvrhi::IDevice& RenderPassPrepareContext::device() const noexcept
{
    return m_device;
}

nvrhi::IFramebuffer& RenderPassPrepareContext::framebuffer() const noexcept
{
    return m_framebuffer;
}

const nvrhi::FramebufferInfoEx& RenderPassPrepareContext::framebufferInfo() const noexcept
{
    return m_framebuffer.getFramebufferInfo();
}

size_t RenderPassPrepareContext::frameSlotCount() const noexcept
{
    return m_frame_slot_count;
}

RenderPassContext::RenderPassContext(nvrhi::IDevice& device, nvrhi::ICommandList& commands,
        nvrhi::IFramebuffer& framebuffer, nvrhi::ITexture& color_texture,
        size_t frame_slot_index, uint32_t image_index,
        const detail::ResourceOwner* resource_owner) noexcept
    : m_device(device),
      m_commands(commands),
      m_framebuffer(framebuffer),
      m_color_texture(color_texture),
      m_frame_slot_index(frame_slot_index),
      m_image_index(image_index),
      m_resource_owner(resource_owner)
{}

nvrhi::IDevice& RenderPassContext::device() const noexcept
{
    return m_device;
}

nvrhi::ICommandList& RenderPassContext::commands() const noexcept
{
    return m_commands;
}

nvrhi::IFramebuffer& RenderPassContext::framebuffer() const noexcept
{
    return m_framebuffer;
}

nvrhi::ITexture& RenderPassContext::colorTexture() const noexcept
{
    return m_color_texture;
}

const nvrhi::FramebufferInfoEx& RenderPassContext::framebufferInfo() const noexcept
{
    return m_framebuffer.getFramebufferInfo();
}

size_t RenderPassContext::frameSlotIndex() const noexcept
{
    return m_frame_slot_index;
}

uint32_t RenderPassContext::imageIndex() const noexcept
{
    return m_image_index;
}

nvrhi::IBuffer& RenderPassContext::buffer(const VertexBuffer& buffer) const
{
    if (!detail::BufferAccess::isOwnedBy(buffer, m_resource_owner)) {
        throw std::invalid_argument("The vertex buffer belongs to another RenderDevice.");
    }
    return detail::BufferAccess::nvrhiHandle(buffer);
}

nvrhi::IBuffer& RenderPassContext::buffer(const IndexBuffer& buffer) const
{
    if (!detail::BufferAccess::isOwnedBy(buffer, m_resource_owner)) {
        throw std::invalid_argument("The index buffer belongs to another RenderDevice.");
    }
    return detail::BufferAccess::nvrhiHandle(buffer);
}

nvrhi::ITexture& RenderPassContext::texture(const Texture2D& texture) const
{
    if (!detail::TextureAccess::isOwnedBy(texture, m_resource_owner)) {
        throw std::invalid_argument("The texture belongs to another RenderDevice.");
    }
    return detail::TextureAccess::nvrhiHandle(texture);
}

nvrhi::ITexture& RenderPassContext::texture(const TextureCube& texture) const
{
    if (!detail::TextureAccess::isOwnedBy(texture, m_resource_owner)) {
        throw std::invalid_argument("The cube texture belongs to another RenderDevice.");
    }
    return detail::TextureAccess::nvrhiHandle(texture);
}

} // namespace arti::renderer
