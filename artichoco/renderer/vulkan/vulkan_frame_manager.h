#pragma once

#include "artichoco/core/window.h"
#include "vulkan_device.h"
#include "vulkan_surface.h"
#include "vulkan_swapchain.h"

#include <nvrhi/nvrhi.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace arti::renderer::vulkan {

class NvrhiVulkanDevice;

class NvrhiFrameContext {
public:
    nvrhi::ICommandList& commands() const noexcept;
    nvrhi::IFramebuffer& framebuffer() const noexcept;
    nvrhi::ITexture& colorTexture() const noexcept;
    const nvrhi::FramebufferInfoEx& framebufferInfo() const noexcept;
    size_t frameSlotIndex() const noexcept;
    uint32_t imageIndex() const noexcept;

private:
    friend class VulkanFrameManager;

    NvrhiFrameContext(nvrhi::ICommandList& commands, nvrhi::IFramebuffer& framebuffer,
            nvrhi::ITexture& color_texture, size_t frame_slot_index,
            uint32_t image_index) noexcept;

    nvrhi::ICommandList& m_commands;
    nvrhi::IFramebuffer& m_framebuffer;
    nvrhi::ITexture& m_color_texture;
    size_t m_frame_slot_index{0};
    uint32_t m_image_index{0};
};

struct NvrhiFrameResult {
    std::optional<size_t> completed_frame_slot;
    std::optional<size_t> submitted_frame_slot;
    bool rendered{false};
};

using NvrhiClearFrameResult = NvrhiFrameResult;

class VulkanFrameManager {
public:
    VulkanFrameManager(core::Window& window, const VulkanDevice& device,
            NvrhiVulkanDevice& nvrhi_device, const VulkanSurface& surface,
            uint32_t frames_in_flight, bool vsync);
    ~VulkanFrameManager();

    VulkanFrameManager(const VulkanFrameManager&) = delete;
    VulkanFrameManager& operator=(const VulkanFrameManager&) = delete;

    NvrhiFrameResult renderNvrhiFrame(
            const std::function<void(NvrhiFrameContext&)>& record);
    NvrhiClearFrameResult renderNvrhiClearFrame(const nvrhi::Color& clear_color);

    void requestSwapchainRecreation() noexcept;
    void setVsync(bool enabled) noexcept;
    bool vsync() const noexcept;
    vk::PresentModeKHR presentMode() const noexcept;
    void waitIdle() const;
    size_t frameSlotCount() const noexcept;
    uint32_t swapchainWidth() const noexcept;
    uint32_t swapchainHeight() const noexcept;
    bool swapchainIsRenderable() const noexcept;

private:
    void recoverAbandonedFrame();
    bool recreateSwapchain();
    void createRenderFinishedSemaphores();

    core::Window& m_window;
    const VulkanDevice& m_device;
    NvrhiVulkanDevice& m_nvrhi_device;
    VulkanSwapchain m_swapchain;
    nvrhi::CommandListHandle m_nvrhi_command_list;
    std::vector<nvrhi::EventQueryHandle> m_frame_queries;
    std::vector<vk::raii::Semaphore> m_image_available_semaphores;
    std::vector<vk::raii::Semaphore> m_render_finished_semaphores;
    std::vector<bool> m_frame_slot_submitted;
    size_t m_current_frame_slot{0};
    bool m_recreate_swapchain{false};
    bool m_frame_active{false};
    bool m_recovery_required{false};
};

} // namespace arti::renderer::vulkan
