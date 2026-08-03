#include "renderer.h"

#include "buffer_access.h"
#include "texture_access.h"
#include "artichoco/renderer/renderer_log.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vulkan/frame_resources.h"
#include "artichoco/renderer/vulkan/vulkan_allocator.h"
#include "artichoco/renderer/vulkan/vulkan_context.h"
#include "artichoco/renderer/vulkan/vulkan_depth_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_device.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_shader.h"
#include "artichoco/renderer/vulkan/vulkan_surface.h"
#include "artichoco/renderer/vulkan/vulkan_surface_source.h"
#include "artichoco/renderer/vulkan/vulkan_swapchain.h"
#include "artichoco/renderer/vulkan/vulkan_texture_descriptors.h"
#include "artichoco/renderer/vulkan/vulkan_upload_context.h"

#include <algorithm>
#include <array>
#include <future>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

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

vk::IndexType toVulkanIndexType(IndexType type)
{
    switch (type) {
    case IndexType::UInt16:
        return vk::IndexType::eUint16;
    case IndexType::UInt32:
        return vk::IndexType::eUint32;
    }
    throw std::invalid_argument("Unsupported index type.");
}

} // namespace

struct Renderer::Impl final
    : detail::DeferredResourceOwner,
      std::enable_shared_from_this<Renderer::Impl> {
    Impl(
        core::Window& window,
        std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
        const RendererCreateInfo& info);
    ~Impl();

    void deferRelease(std::packaged_task<void()> release) override;
    void shutdown();

    VertexBuffer createVertexBuffer(
        std::span<const std::byte> data,
        uint32_t vertex_count,
        VertexBufferLayout layout);
    IndexBuffer createIndexBuffer(
        std::span<const std::byte> data,
        uint32_t index_count,
        IndexType index_type);
    Texture2D createTexture2D(
        std::span<const std::byte> rgba_pixels,
        uint32_t width,
        uint32_t height,
        TextureFormat format);
    bool renderFrame(std::span<const DrawCommand> draw_commands);
    void requestSwapchainRecreation() noexcept;
    void setClearColor(const std::array<float, 4>& color) noexcept;
    void waitIdle() const;

private:
    bool recreateSwapchain();
    void createRenderFinishedSemaphores();
    void createDepthBuffers();
    void completeFrame(size_t frame_index);
    void markFrameSubmitted(size_t frame_index);
    void prepareDrawCommands(std::span<const DrawCommand> draw_commands);
    void recordCommands(
        vulkan::FrameResources& frame,
        vulkan::VulkanDepthBuffer& depth_buffer,
        uint32_t image_index,
        std::span<const DrawCommand> draw_commands) const;

    core::Window& m_window;
    std::unique_ptr<vulkan::VulkanSurfaceSource> m_surface_source;
    vulkan::VulkanContext m_context;
    vulkan::VulkanSurface m_surface;
    vulkan::VulkanDevice m_device;
    vulkan::VulkanAllocator m_allocator;
    vulkan::VulkanUploadContext m_upload_context;
    vulkan::VulkanTextureDescriptors m_texture_descriptors;
    vulkan::VulkanSwapchain m_swapchain;
    std::unique_ptr<vulkan::VulkanShader> m_shader;
    std::unique_ptr<vulkan::VulkanPipeline> m_pipeline;
    std::vector<vk::raii::Semaphore> m_render_finished_semaphores;
    std::vector<vulkan::FrameResources> m_frames;
    std::vector<vulkan::VulkanDepthBuffer> m_depth_buffers;
    vk::ClearColorValue m_clear_color;
    size_t m_current_frame{0};
    bool m_recreate_swapchain{false};
    bool m_shutdown{false};

    struct PendingRelease {
        std::packaged_task<void()> release;
        std::vector<bool> waiting_for_frames;
        size_t remaining_frames{0};
    };
    std::mutex m_release_mutex;
    std::vector<bool> m_frame_submitted;
    std::vector<PendingRelease> m_pending_releases;
};

Renderer::Impl::Impl(
    core::Window& window,
    std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
    const RendererCreateInfo& info)
    : m_window(window),
      m_surface_source(std::move(surface_source)),
      m_context(makeContextCreateInfo(info), requireSurfaceSource(m_surface_source)),
      m_surface(m_context.instance(), requireSurfaceSource(m_surface_source)),
      m_device(m_context.instance(), m_surface.handle()),
      m_allocator(m_context, m_device),
      m_upload_context(m_device, m_allocator),
      m_texture_descriptors(m_device),
      m_swapchain(m_window, m_device, m_surface),
      m_clear_color(vk::ClearColorValue{info.clear_color})
{
    if (info.frames_in_flight == 0) {
        throw std::invalid_argument("Renderer requires at least one frame in flight.");
    }

    m_frames.reserve(info.frames_in_flight);
    for (uint32_t index = 0; index < info.frames_in_flight; ++index) {
        m_frames.emplace_back(m_device);
    }
    m_frame_submitted.resize(m_frames.size(), false);

    auto program = SlangCompiler::compileGraphics(
        info.shader_path, info.vertex_entry_point, info.fragment_entry_point);
    m_shader = std::make_unique<vulkan::VulkanShader>(
        m_device,
        program.vertex.spirv,
        std::move(program.vertex.entry_point),
        program.fragment.spirv,
        std::move(program.fragment.entry_point));

    createDepthBuffers();
    createRenderFinishedSemaphores();
    getLogChannel().info("Initialized Vulkan renderer with {} frames in flight", m_frames.size());
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
    std::scoped_lock lock{m_release_mutex};
    PendingRelease pending;
    pending.release = std::move(release);
    pending.waiting_for_frames = m_frame_submitted;
    pending.remaining_frames = static_cast<size_t>(std::ranges::count(m_frame_submitted, true));
    if (m_shutdown) {
        pending.release();
        return;
    }
    m_pending_releases.push_back(std::move(pending));
}

void Renderer::Impl::shutdown()
{
    if (m_shutdown) {
        return;
    }
    waitIdle();
    std::scoped_lock lock{m_release_mutex};
    m_shutdown = true;
    std::ranges::fill(m_frame_submitted, false);
    for (auto& pending : m_pending_releases) {
        pending.release();
    }
    m_pending_releases.clear();
}

VertexBuffer Renderer::Impl::createVertexBuffer(
    std::span<const std::byte> data,
    uint32_t vertex_count,
    VertexBufferLayout layout)
{
    return detail::BufferAccess::createVertexBuffer(
        m_allocator, m_upload_context, shared_from_this(), data, vertex_count, std::move(layout));
}

IndexBuffer Renderer::Impl::createIndexBuffer(
    std::span<const std::byte> data,
    uint32_t index_count,
    IndexType index_type)
{
    return detail::BufferAccess::createIndexBuffer(
        m_allocator, m_upload_context, shared_from_this(), data, index_count, index_type);
}

Texture2D Renderer::Impl::createTexture2D(
    std::span<const std::byte> rgba_pixels,
    uint32_t width,
    uint32_t height,
    TextureFormat format)
{
    return detail::TextureAccess::create(
        m_allocator,
        m_upload_context,
        m_texture_descriptors,
        shared_from_this(),
        rgba_pixels,
        width,
        height,
        format);
}

bool Renderer::Impl::renderFrame(std::span<const DrawCommand> draw_commands)
{
    if (m_window.getFramebufferWidth() == 0 || m_window.getFramebufferHeight() == 0) {
        m_recreate_swapchain = true;
        return false;
    }

    const auto extent = m_swapchain.extent();
    if (extent.width != m_window.getFramebufferWidth() || extent.height != m_window.getFramebufferHeight()) {
        m_recreate_swapchain = true;
    }
    if (m_recreate_swapchain && !recreateSwapchain()) {
        return false;
    }

    prepareDrawCommands(draw_commands);
    auto& frame = m_frames[m_current_frame];
    auto& depth_buffer = m_depth_buffers.at(m_current_frame);
    const std::array fences = {*frame.inFlightFence()};
    if (m_device.device().waitForFences(fences, true, std::numeric_limits<uint64_t>::max()) != vk::Result::eSuccess) {
        throw std::runtime_error("Timed out while waiting for a Vulkan frame fence.");
    }
    completeFrame(m_current_frame);

    uint32_t image_index = 0;
    bool acquire_suboptimal = false;
    try {
        const auto acquired = m_swapchain.handle().acquireNextImage(
            std::numeric_limits<uint64_t>::max(), *frame.imageAvailableSemaphore(), nullptr);
        image_index = acquired.value;
        acquire_suboptimal = acquired.result == vk::Result::eSuboptimalKHR;
    } catch (const vk::OutOfDateKHRError&) {
        m_recreate_swapchain = true;
        recreateSwapchain();
        return false;
    }

    m_device.device().resetFences(fences);
    frame.commandPool().reset();
    recordCommands(frame, depth_buffer, image_index, draw_commands);

    vk::SemaphoreSubmitInfo wait_semaphore{};
    wait_semaphore.setSemaphore(*frame.imageAvailableSemaphore())
        .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    vk::CommandBufferSubmitInfo command_buffer{};
    command_buffer.setCommandBuffer(*frame.commandBuffer());
    const auto& render_finished = m_render_finished_semaphores.at(image_index);
    vk::SemaphoreSubmitInfo signal_semaphore{};
    signal_semaphore.setSemaphore(*render_finished)
        .setStageMask(vk::PipelineStageFlagBits2::eAllGraphics);

    vk::SubmitInfo2 submit_info{};
    submit_info.setWaitSemaphoreInfos(wait_semaphore)
        .setCommandBufferInfos(command_buffer)
        .setSignalSemaphoreInfos(signal_semaphore);
    const std::array submits = {submit_info};
    if (m_device.usesCore13()) {
        m_device.graphicsQueue().submit2(submits, *frame.inFlightFence());
    } else {
        m_device.graphicsQueue().submit2KHR(submits, *frame.inFlightFence());
    }
    markFrameSubmitted(m_current_frame);

    const std::array wait_semaphores = {*render_finished};
    const std::array swapchains = {*m_swapchain.handle()};
    const std::array image_indices = {image_index};
    vk::PresentInfoKHR present_info{};
    present_info.setWaitSemaphores(wait_semaphores)
        .setSwapchains(swapchains)
        .setImageIndices(image_indices);

    bool present_out_of_date = false;
    try {
        const vk::Result result = m_device.presentQueue().presentKHR(present_info);
        m_recreate_swapchain = acquire_suboptimal || result == vk::Result::eSuboptimalKHR;
    } catch (const vk::OutOfDateKHRError&) {
        present_out_of_date = true;
        m_recreate_swapchain = true;
    }

    m_current_frame = (m_current_frame + 1) % m_frames.size();
    if (present_out_of_date) {
        recreateSwapchain();
    }
    return true;
}

void Renderer::Impl::requestSwapchainRecreation() noexcept
{
    m_recreate_swapchain = true;
}

void Renderer::Impl::setClearColor(const std::array<float, 4>& color) noexcept
{
    m_clear_color = vk::ClearColorValue{color};
}

void Renderer::Impl::waitIdle() const
{
    m_device.device().waitIdle();
}

bool Renderer::Impl::recreateSwapchain()
{
    if (m_window.getFramebufferWidth() == 0 || m_window.getFramebufferHeight() == 0) {
        return false;
    }
    waitIdle();
    const bool recreated = m_swapchain.recreate();
    if (recreated) {
        createDepthBuffers();
        if (m_pipeline) {
            const VertexBufferLayout layout = m_pipeline->vertexLayout();
            m_pipeline = std::make_unique<vulkan::VulkanPipeline>(
                m_device,
                *m_shader,
                layout,
                *m_texture_descriptors.layout(),
                m_swapchain.format(),
                m_depth_buffers.front().format());
        }
        createRenderFinishedSemaphores();
    }
    m_recreate_swapchain = !recreated;
    return recreated;
}

void Renderer::Impl::createRenderFinishedSemaphores()
{
    m_render_finished_semaphores.clear();
    m_render_finished_semaphores.reserve(m_swapchain.imageCount());
    for (size_t index = 0; index < m_swapchain.imageCount(); ++index) {
        m_render_finished_semaphores.emplace_back(m_device.device(), vk::SemaphoreCreateInfo{});
    }
}

void Renderer::Impl::createDepthBuffers()
{
    m_depth_buffers.clear();
    if (!m_swapchain.isRenderable()) {
        return;
    }
    m_depth_buffers.reserve(m_frames.size());
    for (size_t index = 0; index < m_frames.size(); ++index) {
        m_depth_buffers.emplace_back(m_device, m_allocator, m_swapchain.extent());
    }
}

void Renderer::Impl::completeFrame(size_t frame_index)
{
    std::scoped_lock lock{m_release_mutex};
    m_frame_submitted.at(frame_index) = false;
    for (auto it = m_pending_releases.begin(); it != m_pending_releases.end();) {
        if (it->waiting_for_frames.at(frame_index)) {
            it->waiting_for_frames[frame_index] = false;
            --it->remaining_frames;
        }
        if (it->remaining_frames == 0) {
            it->release();
            it = m_pending_releases.erase(it);
        } else {
            ++it;
        }
    }
}

void Renderer::Impl::markFrameSubmitted(size_t frame_index)
{
    std::scoped_lock lock{m_release_mutex};
    m_frame_submitted.at(frame_index) = true;
}

void Renderer::Impl::prepareDrawCommands(std::span<const DrawCommand> draw_commands)
{
    if (draw_commands.empty()) {
        return;
    }

    const VertexBufferLayout* layout = nullptr;
    for (const DrawCommand& draw : draw_commands) {
        if (draw.vertex_buffer == nullptr || draw.index_buffer == nullptr || draw.base_color_texture == nullptr) {
            throw std::invalid_argument("A draw command requires vertex, index, and base-color texture resources.");
        }
        if (!detail::BufferAccess::isOwnedBy(*draw.vertex_buffer, this) ||
            !detail::BufferAccess::isOwnedBy(*draw.index_buffer, this) ||
            !detail::TextureAccess::isOwnedBy(*draw.base_color_texture, this)) {
            throw std::invalid_argument("A draw command contains a resource from another Renderer.");
        }
        const uint32_t index_count = draw.index_count == 0 ? draw.index_buffer->indexCount() : draw.index_count;
        if (draw.first_index > draw.index_buffer->indexCount() ||
            index_count > draw.index_buffer->indexCount() - draw.first_index) {
            throw std::out_of_range("A draw command exceeds its index buffer.");
        }
        if (layout == nullptr) {
            layout = &draw.vertex_buffer->layout();
        } else if (*layout != draw.vertex_buffer->layout()) {
            throw std::invalid_argument("The current Renderer supports one vertex layout per frame.");
        }
    }

    if (!m_pipeline || m_pipeline->vertexLayout() != *layout ||
        m_pipeline->colorFormat() != m_swapchain.format() ||
        m_pipeline->depthFormat() != m_depth_buffers.front().format()) {
        m_pipeline = std::make_unique<vulkan::VulkanPipeline>(
            m_device,
            *m_shader,
            *layout,
            *m_texture_descriptors.layout(),
            m_swapchain.format(),
            m_depth_buffers.front().format());
    }
}

void Renderer::Impl::recordCommands(
    vulkan::FrameResources& frame,
    vulkan::VulkanDepthBuffer& depth_buffer,
    uint32_t image_index,
    std::span<const DrawCommand> draw_commands) const
{
    const auto& command_buffer = frame.commandBuffer();
    vk::CommandBufferBeginInfo begin_info{};
    begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    command_buffer.begin(begin_info);

    vk::ImageSubresourceRange color_range{};
    color_range.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    vk::ImageMemoryBarrier2 to_color_attachment{};
    to_color_attachment.setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
        .setSrcAccessMask(vk::AccessFlagBits2::eNone)
        .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
        .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
        .setOldLayout(vk::ImageLayout::eUndefined)
        .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setImage(m_swapchain.image(image_index))
        .setSubresourceRange(color_range);

    vk::ImageSubresourceRange depth_range{};
    depth_range.setAspectMask(vk::ImageAspectFlagBits::eDepth)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    vk::ImageMemoryBarrier2 to_depth_attachment{};
    to_depth_attachment.setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
        .setSrcAccessMask(vk::AccessFlagBits2::eNone)
        .setDstStageMask(
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
        .setDstAccessMask(
            vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
        .setOldLayout(vk::ImageLayout::eUndefined)
        .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
        .setImage(depth_buffer.image())
        .setSubresourceRange(depth_range);

    const std::array attachment_barriers = {to_color_attachment, to_depth_attachment};
    vk::DependencyInfo attachment_dependency{};
    attachment_dependency.setImageMemoryBarriers(attachment_barriers);
    if (m_device.usesCore13()) {
        command_buffer.pipelineBarrier2(attachment_dependency);
    } else {
        command_buffer.pipelineBarrier2KHR(attachment_dependency);
    }

    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.setImageView(*m_swapchain.imageView(image_index))
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearValue{m_clear_color});
    vk::RenderingAttachmentInfo depth_attachment{};
    depth_attachment.setImageView(*depth_buffer.imageView())
        .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setClearValue(vk::ClearValue{vk::ClearDepthStencilValue{1.0f, 0}});

    const vk::Extent2D render_extent = m_swapchain.extent();
    vk::RenderingInfo rendering_info{};
    rendering_info.setRenderArea(vk::Rect2D{{0, 0}, render_extent})
        .setLayerCount(1)
        .setColorAttachments(color_attachment)
        .setPDepthAttachment(&depth_attachment);
    if (m_device.usesCore13()) {
        command_buffer.beginRendering(rendering_info);
    } else {
        command_buffer.beginRenderingKHR(rendering_info);
    }

    const std::array viewports = {
        vk::Viewport{
            0.0f,
            0.0f,
            static_cast<float>(render_extent.width),
            static_cast<float>(render_extent.height),
            0.0f,
            1.0f},
    };
    const std::array scissors = {vk::Rect2D{{0, 0}, render_extent}};
    command_buffer.setViewport(0, viewports);
    command_buffer.setScissor(0, scissors);

    if (!draw_commands.empty()) {
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline->handle());
    }
    for (const DrawCommand& draw : draw_commands) {
        const std::array vertex_buffers = {detail::BufferAccess::handle(*draw.vertex_buffer)};
        constexpr std::array<vk::DeviceSize, 1> offsets = {0};
        command_buffer.bindVertexBuffers(0, vertex_buffers, offsets);
        command_buffer.bindIndexBuffer(
            detail::BufferAccess::handle(*draw.index_buffer), 0, toVulkanIndexType(draw.index_buffer->indexType()));
        const std::array descriptor_sets = {detail::TextureAccess::descriptorSet(*draw.base_color_texture)};
        command_buffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *m_pipeline->layout(), 0, descriptor_sets, {});
        command_buffer.pushConstants<float>(
            *m_pipeline->layout(), vk::ShaderStageFlagBits::eVertex, 0, draw.transform);
        const uint32_t index_count = draw.index_count == 0 ? draw.index_buffer->indexCount() : draw.index_count;
        command_buffer.drawIndexed(index_count, 1, draw.first_index, draw.vertex_offset, 0);
    }

    if (m_device.usesCore13()) {
        command_buffer.endRendering();
    } else {
        command_buffer.endRenderingKHR();
    }

    vk::ImageMemoryBarrier2 to_present{};
    to_present.setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
        .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eNone)
        .setDstAccessMask(vk::AccessFlagBits2::eNone)
        .setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
        .setImage(m_swapchain.image(image_index))
        .setSubresourceRange(color_range);
    vk::DependencyInfo present_dependency{};
    present_dependency.setImageMemoryBarriers(to_present);
    if (m_device.usesCore13()) {
        command_buffer.pipelineBarrier2(present_dependency);
    } else {
        command_buffer.pipelineBarrier2KHR(present_dependency);
    }

    command_buffer.end();
}

Renderer::Renderer(
    core::Window& window,
    std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
    const RendererCreateInfo& info)
    : m_impl(std::make_shared<Impl>(window, std::move(surface_source), info))
{}

Renderer::~Renderer()
{
    m_impl->shutdown();
}

VertexBuffer Renderer::createVertexBuffer(
    std::span<const std::byte> data,
    uint32_t vertex_count,
    VertexBufferLayout layout)
{
    return m_impl->createVertexBuffer(data, vertex_count, std::move(layout));
}

IndexBuffer Renderer::createIndexBuffer(
    std::span<const std::byte> data,
    uint32_t index_count,
    IndexType index_type)
{
    return m_impl->createIndexBuffer(data, index_count, index_type);
}

Texture2D Renderer::createTexture2D(
    std::span<const std::byte> rgba_pixels,
    uint32_t width,
    uint32_t height,
    TextureFormat format)
{
    return m_impl->createTexture2D(rgba_pixels, width, height, format);
}

bool Renderer::renderFrame(std::span<const DrawCommand> draw_commands)
{
    return m_impl->renderFrame(draw_commands);
}

void Renderer::requestSwapchainRecreation() noexcept
{
    m_impl->requestSwapchainRecreation();
}

void Renderer::setClearColor(const std::array<float, 4>& color) noexcept
{
    m_impl->setClearColor(color);
}

void Renderer::waitIdle() const
{
    m_impl->waitIdle();
}

} // namespace arti::renderer
