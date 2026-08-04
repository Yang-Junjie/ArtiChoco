#include "vulkan_frame_manager.h"

#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace arti::renderer::vulkan {

VulkanFrameContext::VulkanFrameContext(const VulkanDevice& device,
                                       const vk::raii::CommandBuffer& command_buffer,
                                       size_t frame_slot_index,
                                       uint32_t image_index,
                                       vk::Extent2D extent,
                                       vk::Format color_format,
                                       vk::Format depth_format,
                                       vk::Image color_image,
                                       vk::ImageView color_image_view,
                                       vk::Image depth_image,
                                       vk::ImageView depth_image_view,
                                       bool acquire_suboptimal) noexcept
    : m_commands(device, command_buffer),
      m_frame_slot_index(frame_slot_index),
      m_image_index(image_index),
      m_extent(extent),
      m_color_format(color_format),
      m_depth_format(depth_format),
      m_color_image(color_image),
      m_color_image_view(color_image_view),
      m_depth_image(depth_image),
      m_depth_image_view(depth_image_view),
      m_acquire_suboptimal(acquire_suboptimal)
{}

size_t VulkanFrameContext::frameSlotIndex() const noexcept
{
    return m_frame_slot_index;
}

uint32_t VulkanFrameContext::imageIndex() const noexcept
{
    return m_image_index;
}

vk::Extent2D VulkanFrameContext::extent() const noexcept
{
    return m_extent;
}

vk::Format VulkanFrameContext::colorFormat() const noexcept
{
    return m_color_format;
}

vk::Format VulkanFrameContext::depthFormat() const noexcept
{
    return m_depth_format;
}

vk::Image VulkanFrameContext::colorImage() const noexcept
{
    return m_color_image;
}

vk::ImageView VulkanFrameContext::colorImageView() const noexcept
{
    return m_color_image_view;
}

vk::Image VulkanFrameContext::depthImage() const noexcept
{
    return m_depth_image;
}

vk::ImageView VulkanFrameContext::depthImageView() const noexcept
{
    return m_depth_image_view;
}

VulkanCommandRecorder& VulkanFrameContext::commands() noexcept
{
    return m_commands;
}

VulkanFrameToken::VulkanFrameToken(VulkanFrameManager& manager, VulkanFrameContext context) noexcept
    : m_manager(&manager),
      m_context(std::move(context))
{}

VulkanFrameToken::~VulkanFrameToken()
{
    if (m_manager != nullptr) {
        m_manager->abandonFrame(*this);
    }
}

VulkanFrameToken::VulkanFrameToken(VulkanFrameToken&& other) noexcept
    : m_manager(std::exchange(other.m_manager, nullptr)),
      m_context(std::move(other.m_context))
{}

VulkanFrameContext& VulkanFrameToken::context() noexcept
{
    return m_context;
}

size_t VulkanFrameToken::submit()
{
    if (m_manager == nullptr) {
        throw std::logic_error("A Vulkan frame can only be submitted once.");
    }
    return m_manager->submitAndPresent(*this);
}

VulkanFrameManager::VulkanFrameManager(core::Window& window,
                                       const VulkanDevice& device,
                                       VulkanAllocator& allocator,
                                       const VulkanSurface& surface,
                                       uint32_t frames_in_flight)
    : m_window(window),
      m_device(device),
      m_allocator(allocator),
      m_swapchain(window, device, surface)
{
    if (frames_in_flight == 0) {
        throw std::invalid_argument("VulkanFrameManager requires at least one frame in flight.");
    }

    m_frame_slots.reserve(frames_in_flight);
    for (uint32_t index = 0; index < frames_in_flight; ++index) {
        m_frame_slots.emplace_back(device);
    }
    m_frame_slot_submitted.resize(m_frame_slots.size(), false);
    createDepthBuffers();
    createRenderFinishedSemaphores();
}

VulkanFrameManager::~VulkanFrameManager()
{
    try {
        waitIdle();
    } catch (...) {
    }
}

VulkanFrameBeginResult VulkanFrameManager::beginFrame()
{
    if (m_frame_active) {
        throw std::logic_error("The active Vulkan frame must be submitted before beginning another frame.");
    }

    if (m_recovery_required) {
        recoverAbandonedFrame();
    }

    VulkanFrameBeginResult result;
    if (m_window.getFramebufferWidth() == 0 || m_window.getFramebufferHeight() == 0) {
        m_recreate_swapchain = true;
        return result;
    }

    const vk::Extent2D current_extent = m_swapchain.extent();
    if (current_extent.width != m_window.getFramebufferWidth() ||
        current_extent.height != m_window.getFramebufferHeight()) {
        m_recreate_swapchain = true;
    }
    if (m_recreate_swapchain && !recreateSwapchain()) {
        return result;
    }

    auto& frame_slot = m_frame_slots.at(m_current_frame_slot);
    const std::array fences = {*frame_slot.inFlightFence()};
    if (m_device.device().waitForFences(fences, true, std::numeric_limits<uint64_t>::max()) != vk::Result::eSuccess) {
        throw std::runtime_error("Timed out while waiting for a Vulkan frame fence.");
    }
    if (m_frame_slot_submitted.at(m_current_frame_slot)) {
        result.completed_frame_slot = m_current_frame_slot;
        m_frame_slot_submitted[m_current_frame_slot] = false;
    }

    uint32_t image_index = 0;
    bool acquire_suboptimal = false;
    try {
        const auto acquired = m_swapchain.handle().acquireNextImage(
            std::numeric_limits<uint64_t>::max(), *frame_slot.imageAvailableSemaphore(), nullptr);
        image_index = acquired.value;
        acquire_suboptimal = acquired.result == vk::Result::eSuboptimalKHR;
    } catch (const vk::OutOfDateKHRError&) {
        m_recreate_swapchain = true;
        recreateSwapchain();
        return result;
    }

    m_frame_active = true;
    try {
        frame_slot.commandPool().reset();
        auto& depth_buffer = m_depth_buffers.at(m_current_frame_slot);
        result.frame.emplace(VulkanFrameToken{
            *this,
            VulkanFrameContext{
                m_device,
                frame_slot.commandBuffer(),
                m_current_frame_slot,
                image_index,
                m_swapchain.extent(),
                m_swapchain.format(),
                depth_buffer.format(),
                m_swapchain.image(image_index),
                *m_swapchain.imageView(image_index),
                depth_buffer.image(),
                *depth_buffer.imageView(),
                acquire_suboptimal,
            },
        });
        result.frame->context().commands().begin();
    } catch (...) {
        if (result.frame) {
            result.frame.reset();
        } else {
            m_recovery_required = true;
            m_frame_active = false;
            try {
                recoverAbandonedFrame();
            } catch (...) {
            }
        }
        throw;
    }
    return result;
}

size_t VulkanFrameManager::submitAndPresent(VulkanFrameToken& frame_token)
{
    auto& frame = frame_token.m_context;
    if (!m_frame_active || frame.frameSlotIndex() != m_current_frame_slot) {
        throw std::logic_error("Only the active Vulkan frame can be submitted.");
    }

    frame.commands().end();
    auto& frame_slot = m_frame_slots.at(m_current_frame_slot);
    vk::SemaphoreSubmitInfo wait_semaphore{};
    wait_semaphore.setSemaphore(*frame_slot.imageAvailableSemaphore())
        .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    vk::CommandBufferSubmitInfo command_buffer{};
    command_buffer.setCommandBuffer(*frame_slot.commandBuffer());
    const auto& render_finished = m_render_finished_semaphores.at(frame.imageIndex());
    vk::SemaphoreSubmitInfo signal_semaphore{};
    signal_semaphore.setSemaphore(*render_finished).setStageMask(vk::PipelineStageFlagBits2::eAllCommands);

    vk::SubmitInfo2 submit_info{};
    submit_info.setWaitSemaphoreInfos(wait_semaphore)
        .setCommandBufferInfos(command_buffer)
        .setSignalSemaphoreInfos(signal_semaphore);
    const std::array submits = {submit_info};
    const std::array fences = {*frame_slot.inFlightFence()};
    m_device.device().resetFences(fences);
    if (m_device.usesCore13()) {
        m_device.graphicsQueue().submit2(submits, *frame_slot.inFlightFence());
    } else {
        m_device.graphicsQueue().submit2KHR(submits, *frame_slot.inFlightFence());
    }

    const size_t submitted_frame_slot = m_current_frame_slot;
    m_frame_slot_submitted[submitted_frame_slot] = true;

    const std::array wait_semaphores = {*render_finished};
    const std::array swapchains = {*m_swapchain.handle()};
    const std::array image_indices = {frame.imageIndex()};
    vk::PresentInfoKHR present_info{};
    present_info.setWaitSemaphores(wait_semaphores).setSwapchains(swapchains).setImageIndices(image_indices);
    try {
        const vk::Result present_result = m_device.presentQueue().presentKHR(present_info);
        m_recreate_swapchain = frame.m_acquire_suboptimal || present_result == vk::Result::eSuboptimalKHR;
    } catch (const vk::OutOfDateKHRError&) {
        m_recreate_swapchain = true;
    }

    frame_token.m_manager = nullptr;
    m_frame_active = false;
    m_current_frame_slot = (m_current_frame_slot + 1) % m_frame_slots.size();
    return submitted_frame_slot;
}

void VulkanFrameManager::abandonFrame(VulkanFrameToken& frame) noexcept
{
    if (frame.m_manager != this) {
        return;
    }

    frame.m_manager = nullptr;
    m_frame_active = false;
    m_recovery_required = true;
    try {
        recoverAbandonedFrame();
    } catch (...) {
    }
}

void VulkanFrameManager::recoverAbandonedFrame()
{
    VulkanFrameSlot replacement_frame_slot{m_device};
    waitIdle();

    m_frame_slots.at(m_current_frame_slot) = std::move(replacement_frame_slot);
    m_swapchain.invalidate();
    m_depth_buffers.clear();
    m_render_finished_semaphores.clear();
    m_frame_slot_submitted.at(m_current_frame_slot) = false;
    m_recreate_swapchain = true;
    m_recovery_required = false;
}

void VulkanFrameManager::requestSwapchainRecreation() noexcept
{
    m_recreate_swapchain = true;
}

void VulkanFrameManager::waitIdle() const
{
    m_device.device().waitIdle();
}

size_t VulkanFrameManager::frameSlotCount() const noexcept
{
    return m_frame_slots.size();
}

bool VulkanFrameManager::recreateSwapchain()
{
    if (m_window.getFramebufferWidth() == 0 || m_window.getFramebufferHeight() == 0) {
        return false;
    }

    waitIdle();
    const bool recreated = m_swapchain.recreate();
    if (recreated) {
        createDepthBuffers();
        createRenderFinishedSemaphores();
    }
    m_recreate_swapchain = !recreated;
    return recreated;
}

void VulkanFrameManager::createDepthBuffers()
{
    m_depth_buffers.clear();
    if (!m_swapchain.isRenderable()) {
        return;
    }
    m_depth_buffers.reserve(m_frame_slots.size());
    for (size_t index = 0; index < m_frame_slots.size(); ++index) {
        m_depth_buffers.emplace_back(m_device, m_allocator, m_swapchain.extent());
    }
}

void VulkanFrameManager::createRenderFinishedSemaphores()
{
    m_render_finished_semaphores.clear();
    m_render_finished_semaphores.reserve(m_swapchain.imageCount());
    for (size_t index = 0; index < m_swapchain.imageCount(); ++index) {
        m_render_finished_semaphores.emplace_back(m_device.device(), vk::SemaphoreCreateInfo{});
    }
}

} // namespace arti::renderer::vulkan
