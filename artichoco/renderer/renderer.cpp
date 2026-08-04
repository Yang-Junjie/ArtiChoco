#include "artichoco/renderer/detail/deferred_release_queue.h"
#include "artichoco/renderer/detail/deferred_resource_owner.h"
#include "artichoco/renderer/renderer_log.h"
#include "artichoco/renderer/vulkan/vulkan_allocator.h"
#include "artichoco/renderer/vulkan/vulkan_context.h"
#include "artichoco/renderer/vulkan/vulkan_descriptor_allocator.h"
#include "artichoco/renderer/vulkan/vulkan_device.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_pass.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_surface.h"
#include "artichoco/renderer/vulkan/vulkan_surface_source.h"
#include "artichoco/renderer/vulkan/vulkan_upload_context.h"
#include "buffer_access.h"
#include "renderer.h"
#include "texture_access.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace arti::renderer {
namespace {

vulkan::VulkanSurfaceSource& requireSurfaceSource(const std::unique_ptr<vulkan::VulkanSurfaceSource>& source)
{
    if (!source) {
        throw std::invalid_argument("Renderer requires a surface source.");
    }
    return *source;
}

vulkan::VulkanContextCreateInfo makeContextCreateInfo(const RendererCreateInfo& info)
{
    vulkan::VulkanContextCreateInfo context_info;
    context_info.application_name = info.application_name;
    context_info.enable_validation = info.enable_validation;
    return context_info;
}

} // namespace

struct Renderer::Impl final : detail::DeferredResourceOwner, std::enable_shared_from_this<Renderer::Impl> {
    Impl(core::Window& window,
         std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
         const RendererCreateInfo& info);
    ~Impl();

    void deferRelease(std::packaged_task<void()> release) override;
    void shutdown();

    VertexBuffer createVertexBuffer(std::span<const std::byte> data, uint32_t vertex_count, VertexBufferLayout layout);
    IndexBuffer createIndexBuffer(std::span<const std::byte> data, uint32_t index_count, IndexType index_type);
    Texture2D
        createTexture2D(std::span<const std::byte> rgba_pixels, uint32_t width, uint32_t height, TextureFormat format);
    bool renderFrame(std::span<vulkan::VulkanPass* const> passes);
    void requestSwapchainRecreation() noexcept;
    void waitIdle() const;

private:
    std::unique_ptr<vulkan::VulkanSurfaceSource> m_surface_source;
    vulkan::VulkanContext m_context;
    vulkan::VulkanSurface m_surface;
    vulkan::VulkanDevice m_device;
    vulkan::VulkanAllocator m_allocator;
    vulkan::VulkanUploadContext m_upload_context;
    vulkan::VulkanDescriptorAllocator m_descriptor_allocator;
    vulkan::VulkanFrameManager m_frame_manager;
    detail::DeferredReleaseQueue m_release_queue;
    bool m_shutdown{false};
};

Renderer::Impl::Impl(core::Window& window,
                     std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
                     const RendererCreateInfo& info)
    : m_surface_source(std::move(surface_source)),
      m_context(makeContextCreateInfo(info), requireSurfaceSource(m_surface_source)),
      m_surface(m_context.instance(), requireSurfaceSource(m_surface_source)),
      m_device(m_context.instance(), m_surface.handle()),
      m_allocator(m_context, m_device),
      m_upload_context(m_device, m_allocator),
      m_descriptor_allocator(m_device),
      m_frame_manager(window, m_device, m_allocator, m_surface, info.frames_in_flight),
      m_release_queue(m_frame_manager.frameSlotCount())
{
    getLogChannel().info("Initialized Vulkan renderer with {} frames in flight", m_frame_manager.frameSlotCount());
}

Renderer::Impl::~Impl()
{
    try {
        shutdown();
    } catch (...) {
    }
}

void Renderer::Impl::deferRelease(std::packaged_task<void()> release)
{
    m_release_queue.defer(std::move(release));
}

void Renderer::Impl::shutdown()
{
    if (m_shutdown) {
        return;
    }
    waitIdle();
    m_release_queue.shutdown();
    m_shutdown = true;
}

VertexBuffer Renderer::Impl::createVertexBuffer(std::span<const std::byte> data,
                                                uint32_t vertex_count,
                                                VertexBufferLayout layout)
{
    return detail::BufferAccess::createVertexBuffer(
        m_allocator, m_upload_context, shared_from_this(), data, vertex_count, std::move(layout));
}

IndexBuffer
    Renderer::Impl::createIndexBuffer(std::span<const std::byte> data, uint32_t index_count, IndexType index_type)
{
    return detail::BufferAccess::createIndexBuffer(
        m_allocator, m_upload_context, shared_from_this(), data, index_count, index_type);
}

Texture2D Renderer::Impl::createTexture2D(std::span<const std::byte> rgba_pixels,
                                          uint32_t width,
                                          uint32_t height,
                                          TextureFormat format)
{
    return detail::TextureAccess::create(
        m_allocator, m_upload_context, m_device, shared_from_this(), rgba_pixels, width, height, format);
}

bool Renderer::Impl::renderFrame(std::span<vulkan::VulkanPass* const> passes)
{
    if (passes.empty() || std::ranges::any_of(passes, [](const vulkan::VulkanPass* pass) {
            return pass == nullptr;
        })) {
        throw std::invalid_argument("Renderer requires at least one valid Vulkan pass.");
    }

    vulkan::VulkanPassPrepareContext prepare_context{
        m_device,
        m_allocator,
        m_upload_context,
        m_descriptor_allocator,
        m_frame_manager.frameSlotCount(),
    };
    for (vulkan::VulkanPass* pass : passes) {
        pass->prepare(prepare_context);
    }

    auto begin_result = m_frame_manager.beginFrame();
    if (begin_result.completed_frame_slot) {
        m_release_queue.onFrameSlotCompleted(*begin_result.completed_frame_slot);
    }
    if (!begin_result.frame) {
        return false;
    }

    auto& frame = *begin_result.frame;
    vulkan::VulkanPassContext pass_context{frame.context(), this};
    for (vulkan::VulkanPass* pass : passes) {
        pass->record(pass_context);
    }
    const size_t submitted_frame_slot = frame.submit();
    m_release_queue.onFrameSlotSubmitted(submitted_frame_slot);
    return true;
}

void Renderer::Impl::requestSwapchainRecreation() noexcept
{
    m_frame_manager.requestSwapchainRecreation();
}

void Renderer::Impl::waitIdle() const
{
    m_frame_manager.waitIdle();
}

Renderer::Renderer(core::Window& window,
                   std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
                   const RendererCreateInfo& info)
    : m_impl(std::make_shared<Impl>(window, std::move(surface_source), info))
{}

Renderer::~Renderer()
{
    m_impl->shutdown();
}

VertexBuffer
    Renderer::createVertexBuffer(std::span<const std::byte> data, uint32_t vertex_count, VertexBufferLayout layout)
{
    return m_impl->createVertexBuffer(data, vertex_count, std::move(layout));
}

IndexBuffer Renderer::createIndexBuffer(std::span<const std::byte> data, uint32_t index_count, IndexType index_type)
{
    return m_impl->createIndexBuffer(data, index_count, index_type);
}

Texture2D Renderer::createTexture2D(std::span<const std::byte> rgba_pixels,
                                    uint32_t width,
                                    uint32_t height,
                                    TextureFormat format)
{
    return m_impl->createTexture2D(rgba_pixels, width, height, format);
}

bool Renderer::renderFrame(std::span<vulkan::VulkanPass* const> passes)
{
    return m_impl->renderFrame(passes);
}

void Renderer::requestSwapchainRecreation() noexcept
{
    m_impl->requestSwapchainRecreation();
}

void Renderer::waitIdle() const
{
    m_impl->waitIdle();
}

} // namespace arti::renderer
