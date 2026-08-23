#include "vulkan_frame_manager.h"

#include "nvrhi_vulkan_device.h"
#include "vulkan_frame_policy.h"

#include <nvrhi/vulkan.h>

#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace arti::renderer::vulkan {

NvrhiFrameContext::NvrhiFrameContext(nvrhi::ICommandList& commands,
        nvrhi::IFramebuffer& framebuffer, nvrhi::ITexture& color_texture,
        size_t frame_slot_index, uint32_t image_index) noexcept
    : m_commands(commands),
      m_framebuffer(framebuffer),
      m_color_texture(color_texture),
      m_frame_slot_index(frame_slot_index),
      m_image_index(image_index)
{}

nvrhi::ICommandList& NvrhiFrameContext::commands() const noexcept
{
    return m_commands;
}

nvrhi::IFramebuffer& NvrhiFrameContext::framebuffer() const noexcept
{
    return m_framebuffer;
}

nvrhi::ITexture& NvrhiFrameContext::colorTexture() const noexcept
{
    return m_color_texture;
}

const nvrhi::FramebufferInfoEx& NvrhiFrameContext::framebufferInfo() const noexcept
{
    return m_framebuffer.getFramebufferInfo();
}

size_t NvrhiFrameContext::frameSlotIndex() const noexcept
{
    return m_frame_slot_index;
}

uint32_t NvrhiFrameContext::imageIndex() const noexcept
{
    return m_image_index;
}

VulkanFrameManager::VulkanFrameManager(core::Window& window, const VulkanDevice& device,
        NvrhiVulkanDevice& nvrhi_device, const VulkanSurface& surface,
        uint32_t frames_in_flight)
    : m_window(window),
      m_device(device),
      m_nvrhi_device(nvrhi_device),
      m_swapchain(window, device, nvrhi_device, surface),
      m_nvrhi_command_list(nvrhi_device.device().createCommandList())
{
    if (frames_in_flight == 0) {
        throw std::invalid_argument("VulkanFrameManager requires at least one frame in flight.");
    }
    if (!m_nvrhi_command_list) {
        throw std::runtime_error("Failed to create the NVRHI frame command list.");
    }

    m_frame_queries.reserve(frames_in_flight);
    m_image_available_semaphores.reserve(frames_in_flight);
    for (uint32_t index = 0; index < frames_in_flight; ++index) {
        nvrhi::EventQueryHandle query = nvrhi_device.device().createEventQuery();
        if (!query) {
            throw std::runtime_error("Failed to create an NVRHI frame event query.");
        }
        m_frame_queries.push_back(std::move(query));
        m_image_available_semaphores.emplace_back(
                m_device.device(), vk::SemaphoreCreateInfo{});
    }
    m_frame_slot_submitted.resize(frames_in_flight, false);
    createRenderFinishedSemaphores();
}

VulkanFrameManager::~VulkanFrameManager()
{
    try {
        waitIdle();
    } catch (...) {
    }
}

NvrhiFrameResult VulkanFrameManager::renderNvrhiFrame(
        const std::function<void(NvrhiFrameContext&)>& record)
{
    if (!record) {
        throw std::invalid_argument("NVRHI frame recording requires a callback.");
    }
    if (m_frame_active) {
        throw std::logic_error("The active frame must be submitted before beginning another frame.");
    }
    if (m_recovery_required) {
        recoverAbandonedFrame();
    }

    NvrhiFrameResult result;
    const uint32_t framebuffer_width = m_window.getFramebufferWidth();
    const uint32_t framebuffer_height = m_window.getFramebufferHeight();
    const vk::Extent2D current_extent = m_swapchain.extent();
    const FramebufferAction framebuffer_action = evaluateFramebufferState({
            framebuffer_width,
            framebuffer_height,
            current_extent.width,
            current_extent.height,
            m_recreate_swapchain,
    });
    if (framebuffer_action == FramebufferAction::DeferWhileMinimized) {
        m_recreate_swapchain = true;
        return result;
    }
    if (framebuffer_action == FramebufferAction::RecreateSwapchain) {
        m_recreate_swapchain = true;
    }
    if (m_recreate_swapchain && !recreateSwapchain()) {
        return result;
    }

    if (m_frame_slot_submitted.at(m_current_frame_slot)) {
        auto& query = m_frame_queries.at(m_current_frame_slot);
        m_nvrhi_device.device().waitEventQuery(query);
        m_nvrhi_device.device().resetEventQuery(query);
        result.completed_frame_slot = m_current_frame_slot;
        m_frame_slot_submitted[m_current_frame_slot] = false;
    }

    const auto& image_available = m_image_available_semaphores.at(m_current_frame_slot);
    uint32_t image_index = 0;
    SwapchainStatus acquire_status = SwapchainStatus::Success;
    try {
        const auto acquired = m_swapchain.handle().acquireNextImage(
                std::numeric_limits<uint64_t>::max(), *image_available, nullptr);
        image_index = acquired.value;
        if (acquired.result == vk::Result::eSuboptimalKHR) {
            acquire_status = SwapchainStatus::Suboptimal;
        }
    } catch (const vk::OutOfDateKHRError&) {
        m_recreate_swapchain = requiresSwapchainRecreation(
                SwapchainStatus::OutOfDate, SwapchainStatus::Success);
        recreateSwapchain();
        return result;
    }

    m_frame_active = true;
    bool command_list_open = false;
    bool command_list_started = false;
    bool command_list_executed = false;
    bool acquire_wait_queued = false;
    try {
        m_nvrhi_command_list->open();
        command_list_open = true;
        command_list_started = true;
        NvrhiFrameContext frame_context{
                *m_nvrhi_command_list,
                m_swapchain.nvrhiFramebuffer(image_index),
                m_swapchain.nvrhiTexture(image_index),
                m_current_frame_slot,
                image_index,
        };
        record(frame_context);
        m_nvrhi_command_list->close();
        command_list_open = false;

        const auto& render_finished = m_render_finished_semaphores.at(image_index);
        m_nvrhi_device.nativeDevice().queueWaitForSemaphore(nvrhi::CommandQueue::Graphics,
                static_cast<VkSemaphore>(*image_available), 0);
        acquire_wait_queued = true;
        m_nvrhi_device.nativeDevice().queueSignalSemaphore(nvrhi::CommandQueue::Graphics,
                static_cast<VkSemaphore>(*render_finished), 0);
        m_nvrhi_device.device().executeCommandList(
                m_nvrhi_command_list, nvrhi::CommandQueue::Graphics);
        command_list_executed = true;
        m_nvrhi_device.device().setEventQuery(
                m_frame_queries.at(m_current_frame_slot), nvrhi::CommandQueue::Graphics);
        m_frame_slot_submitted[m_current_frame_slot] = true;
        result.submitted_frame_slot = m_current_frame_slot;

        const std::array wait_semaphores = {*render_finished};
        const std::array swapchains = {*m_swapchain.handle()};
        const std::array image_indices = {image_index};
        vk::PresentInfoKHR present_info{};
        present_info.setWaitSemaphores(wait_semaphores)
                .setSwapchains(swapchains)
                .setImageIndices(image_indices);
        SwapchainStatus present_status = SwapchainStatus::Success;
        try {
            const vk::Result present_result = m_device.presentQueue().presentKHR(present_info);
            if (present_result == vk::Result::eSuboptimalKHR) {
                present_status = SwapchainStatus::Suboptimal;
            }
        } catch (const vk::OutOfDateKHRError&) {
            present_status = SwapchainStatus::OutOfDate;
        }
        m_recreate_swapchain = requiresSwapchainRecreation(
                acquire_status, present_status);

        m_frame_active = false;
        m_current_frame_slot = (m_current_frame_slot + 1) % m_frame_queries.size();
        m_nvrhi_device.device().runGarbageCollection();
        result.rendered = true;
        return result;
    } catch (...) {
        if (command_list_open) {
            try {
                m_nvrhi_command_list->close();
                command_list_open = false;
            } catch (...) {
            }
        }
        if (command_list_started && !command_list_executed && !command_list_open) {
            try {
                if (!acquire_wait_queued) {
                    m_nvrhi_device.nativeDevice().queueWaitForSemaphore(
                            nvrhi::CommandQueue::Graphics,
                            static_cast<VkSemaphore>(*image_available), 0);
                }
                m_nvrhi_device.device().executeCommandList(
                        m_nvrhi_command_list, nvrhi::CommandQueue::Graphics);
                m_nvrhi_device.device().waitForIdle();
            } catch (...) {
            }
        }
        m_frame_active = false;
        m_recovery_required = true;
        try {
            recoverAbandonedFrame();
        } catch (...) {
        }
        throw;
    }
}

NvrhiClearFrameResult VulkanFrameManager::renderNvrhiClearFrame(
        const nvrhi::Color& clear_color)
{
    return renderNvrhiFrame([clear_color](NvrhiFrameContext& frame) {
        frame.commands().clearTextureFloat(
                &frame.colorTexture(), nvrhi::AllSubresources, clear_color);
    });
}

void VulkanFrameManager::recoverAbandonedFrame()
{
    m_nvrhi_command_list = nullptr;
    waitIdle();

    m_nvrhi_command_list = m_nvrhi_device.device().createCommandList();
    if (!m_nvrhi_command_list) {
        throw std::runtime_error("Failed to recreate the NVRHI frame command list.");
    }
    m_image_available_semaphores.at(m_current_frame_slot) =
            vk::raii::Semaphore{m_device.device(), vk::SemaphoreCreateInfo{}};
    m_swapchain.invalidate();
    m_render_finished_semaphores.clear();
    m_frame_slot_submitted[m_current_frame_slot] = false;
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
    m_nvrhi_device.device().runGarbageCollection();
}

size_t VulkanFrameManager::frameSlotCount() const noexcept
{
    return m_frame_queries.size();
}

bool VulkanFrameManager::recreateSwapchain()
{
    if (m_window.getFramebufferWidth() == 0 || m_window.getFramebufferHeight() == 0) {
        return false;
    }

    waitIdle();
    const bool recreated = m_swapchain.recreate();
    if (recreated) {
        createRenderFinishedSemaphores();
    }
    m_recreate_swapchain = !recreated;
    return recreated;
}

void VulkanFrameManager::createRenderFinishedSemaphores()
{
    m_render_finished_semaphores.clear();
    m_render_finished_semaphores.reserve(m_swapchain.imageCount());
    for (size_t index = 0; index < m_swapchain.imageCount(); ++index) {
        m_render_finished_semaphores.emplace_back(
                m_device.device(), vk::SemaphoreCreateInfo{});
    }
}

} // namespace arti::renderer::vulkan
