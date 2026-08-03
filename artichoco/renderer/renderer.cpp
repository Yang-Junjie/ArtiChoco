#include "renderer.h"

#include "artichoco/renderer/renderer_log.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vulkan/frame_resources.h"
#include "artichoco/renderer/vulkan/vulkan_allocator.h"
#include "artichoco/renderer/vulkan/vulkan_context.h"
#include "artichoco/renderer/vulkan/vulkan_device.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_shader.h"
#include "artichoco/renderer/vulkan/vulkan_surface.h"
#include "artichoco/renderer/vulkan/vulkan_surface_source.h"
#include "artichoco/renderer/vulkan/vulkan_swapchain.h"

#include <array>
#include <limits>
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

struct Renderer::Impl {
    Impl(
        core::Window& window,
        std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
        const RendererCreateInfo& info);
    ~Impl();

    bool renderFrame();
    void requestSwapchainRecreation() noexcept;
    void setClearColor(const std::array<float, 4>& color) noexcept;
    void waitIdle() const;

private:
    bool recreateSwapchain();
    void createRenderFinishedSemaphores();
    void recordClearCommands(vulkan::FrameResources& frame, uint32_t image_index) const;

    core::Window& m_window;
    std::unique_ptr<vulkan::VulkanSurfaceSource> m_surface_source;
    vulkan::VulkanContext m_context;
    vulkan::VulkanSurface m_surface;
    vulkan::VulkanDevice m_device;
    vulkan::VulkanAllocator m_allocator;
    vulkan::VulkanSwapchain m_swapchain;
    std::unique_ptr<vulkan::VulkanShader> m_shader;
    std::unique_ptr<vulkan::VulkanPipeline> m_pipeline;
    std::vector<vk::raii::Semaphore> m_render_finished_semaphores;
    std::vector<vulkan::FrameResources> m_frames;
    vk::ClearColorValue m_clear_color;
    size_t m_current_frame{0};
    bool m_recreate_swapchain{false};
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

    auto program = SlangCompiler::compileGraphics(
        info.shader_path, info.vertex_entry_point, info.fragment_entry_point);
    m_shader = std::make_unique<vulkan::VulkanShader>(
        m_device,
        program.vertex.spirv,
        std::move(program.vertex.entry_point),
        program.fragment.spirv,
        std::move(program.fragment.entry_point));
    if (m_swapchain.isRenderable()) {
        m_pipeline = std::make_unique<vulkan::VulkanPipeline>(m_device, *m_shader, m_swapchain.format());
    }

    createRenderFinishedSemaphores();
    getLogChannel().info("Initialized Vulkan renderer with {} frames in flight", m_frames.size());
}

Renderer::Impl::~Impl()
{
    try {
        waitIdle();
    } catch (...) {
    }
}

bool Renderer::Impl::renderFrame()
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

    auto& frame = m_frames[m_current_frame];
    const std::array fences = {*frame.inFlightFence()};
    if (m_device.device().waitForFences(fences, true, std::numeric_limits<uint64_t>::max()) != vk::Result::eSuccess) {
        throw std::runtime_error("Timed out while waiting for a Vulkan frame fence.");
    }

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
    recordClearCommands(frame, image_index);

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
        if (!m_pipeline || m_pipeline->colorFormat() != m_swapchain.format()) {
            m_pipeline = std::make_unique<vulkan::VulkanPipeline>(m_device, *m_shader, m_swapchain.format());
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

void Renderer::Impl::recordClearCommands(vulkan::FrameResources& frame, uint32_t image_index) const
{
    const auto& command_buffer = frame.commandBuffer();
    vk::CommandBufferBeginInfo begin_info{};
    begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    command_buffer.begin(begin_info);

    vk::ImageSubresourceRange range{};
    range.setAspectMask(vk::ImageAspectFlagBits::eColor)
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
                       .setSubresourceRange(range);

    vk::DependencyInfo to_color_dependency{};
    to_color_dependency.setImageMemoryBarriers(to_color_attachment);
    if (m_device.usesCore13()) {
        command_buffer.pipelineBarrier2(to_color_dependency);
    } else {
        command_buffer.pipelineBarrier2KHR(to_color_dependency);
    }

    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.setImageView(*m_swapchain.imageView(image_index))
                    .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                    .setLoadOp(vk::AttachmentLoadOp::eClear)
                    .setStoreOp(vk::AttachmentStoreOp::eStore)
                    .setClearValue(vk::ClearValue{m_clear_color});

    const vk::Extent2D extent = m_swapchain.extent();
    vk::RenderingInfo rendering_info{};
    rendering_info.setRenderArea(vk::Rect2D{{0, 0}, extent})
                  .setLayerCount(1)
                  .setColorAttachments(color_attachment);

    if (m_device.usesCore13()) {
        command_buffer.beginRendering(rendering_info);
    } else {
        command_buffer.beginRenderingKHR(rendering_info);
    }

    if (!m_pipeline) {
        throw std::runtime_error("The Vulkan graphics pipeline is unavailable.");
    }
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline->handle());
    const std::array viewports = {
        vk::Viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f},
    };
    const std::array scissors = {vk::Rect2D{{0, 0}, extent}};
    command_buffer.setViewport(0, viewports);
    command_buffer.setScissor(0, scissors);
    command_buffer.draw(3, 1, 0, 0);

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
              .setSubresourceRange(range);
              
    vk::DependencyInfo to_present_dependency{};
    to_present_dependency.setImageMemoryBarriers(to_present);
    if (m_device.usesCore13()) {
        command_buffer.pipelineBarrier2(to_present_dependency);
    } else {
        command_buffer.pipelineBarrier2KHR(to_present_dependency);
    }

    command_buffer.end();
}

Renderer::Renderer(
    core::Window& window,
    std::unique_ptr<vulkan::VulkanSurfaceSource> surface_source,
    const RendererCreateInfo& info)
    : m_impl(std::make_unique<Impl>(window, std::move(surface_source), info))
{}

Renderer::~Renderer() = default;

bool Renderer::renderFrame()
{
    return m_impl->renderFrame();
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
