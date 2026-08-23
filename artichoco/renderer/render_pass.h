#pragma once

#include "index_buffer.h"
#include "texture_2d.h"
#include "texture_cube.h"
#include "vertex_buffer.h"

#include <nvrhi/nvrhi.h>

#include <cstddef>

namespace arti::renderer::detail {
class ResourceOwner;
}

namespace arti::renderer {

class RenderPassPrepareContext {
public:
    RenderPassPrepareContext(nvrhi::IDevice& device, nvrhi::IFramebuffer& framebuffer,
            size_t frame_slot_count) noexcept;

    nvrhi::IDevice& device() const noexcept;
    nvrhi::IFramebuffer& framebuffer() const noexcept;
    const nvrhi::FramebufferInfoEx& framebufferInfo() const noexcept;
    size_t frameSlotCount() const noexcept;

private:
    nvrhi::IDevice& m_device;
    nvrhi::IFramebuffer& m_framebuffer;
    size_t m_frame_slot_count{0};
};

class RenderPassContext {
public:
    RenderPassContext(nvrhi::IDevice& device, nvrhi::ICommandList& commands,
            nvrhi::IFramebuffer& framebuffer, nvrhi::ITexture& color_texture,
            size_t frame_slot_index, uint32_t image_index,
            const detail::ResourceOwner* resource_owner) noexcept;

    nvrhi::IDevice& device() const noexcept;
    nvrhi::ICommandList& commands() const noexcept;
    nvrhi::IFramebuffer& framebuffer() const noexcept;
    nvrhi::ITexture& colorTexture() const noexcept;
    const nvrhi::FramebufferInfoEx& framebufferInfo() const noexcept;
    size_t frameSlotIndex() const noexcept;
    uint32_t imageIndex() const noexcept;

    nvrhi::IBuffer& buffer(const VertexBuffer& buffer) const;
    nvrhi::IBuffer& buffer(const IndexBuffer& buffer) const;
    nvrhi::ITexture& texture(const Texture2D& texture) const;
    nvrhi::ITexture& texture(const TextureCube& texture) const;

private:
    nvrhi::IDevice& m_device;
    nvrhi::ICommandList& m_commands;
    nvrhi::IFramebuffer& m_framebuffer;
    nvrhi::ITexture& m_color_texture;
    size_t m_frame_slot_index{0};
    uint32_t m_image_index{0};
    const detail::ResourceOwner* m_resource_owner{nullptr};
};

class RenderPass {
public:
    virtual ~RenderPass() = default;

    virtual void prepare(RenderPassPrepareContext&) {}
    virtual void record(RenderPassContext& context) = 0;
};

} // namespace arti::renderer
