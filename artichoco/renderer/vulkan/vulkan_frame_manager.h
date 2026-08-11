#pragma once
#include "artichoco/core/window.h"
#include "vulkan_allocator.h"
#include "vulkan_command_recorder.h"
#include "vulkan_depth_buffer.h"
#include "vulkan_device.h"
#include "vulkan_frame_slot.h"
#include "vulkan_presentation_info.h"
#include "vulkan_surface.h"
#include "vulkan_swapchain.h"

#include <cstddef>

#include <optional>
#include <vector>

namespace arti::renderer::vulkan {

class VulkanFrameManager;

class VulkanFrameContext {
public:
    size_t frameSlotIndex() const noexcept;
    uint32_t imageIndex() const noexcept;
    vk::Extent2D extent() const noexcept;
    vk::Format colorFormat() const noexcept;
    vk::Format depthFormat() const noexcept;
    vk::Image colorImage() const noexcept;
    vk::ImageView colorImageView() const noexcept;
    vk::Image depthImage() const noexcept;
    vk::ImageView depthImageView() const noexcept;
    VulkanCommandRecorder& commands() noexcept;

private:
    friend class VulkanFrameManager;

    VulkanFrameContext(const VulkanDevice& device,
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
                       bool acquire_suboptimal) noexcept;

    VulkanCommandRecorder m_commands;
    size_t m_frame_slot_index{0};
    uint32_t m_image_index{0};
    vk::Extent2D m_extent{};
    vk::Format m_color_format{vk::Format::eUndefined};
    vk::Format m_depth_format{vk::Format::eUndefined};
    vk::Image m_color_image{};
    vk::Image m_depth_image{};
    vk::ImageView m_color_image_view{};
    vk::ImageView m_depth_image_view{};
    bool m_acquire_suboptimal{false};
};

class VulkanFrameToken {
public:
    ~VulkanFrameToken();

    VulkanFrameToken(const VulkanFrameToken&) = delete;
    VulkanFrameToken& operator=(const VulkanFrameToken&) = delete;
    VulkanFrameToken(VulkanFrameToken&& other) noexcept;
    VulkanFrameToken& operator=(VulkanFrameToken&&) = delete;

    VulkanFrameContext& context() noexcept;
    size_t submit();

private:
    friend class VulkanFrameManager;

    VulkanFrameToken(VulkanFrameManager& manager, VulkanFrameContext context) noexcept;

    VulkanFrameManager* m_manager{nullptr};
    VulkanFrameContext m_context;
};

struct VulkanFrameBeginResult {
    std::optional<size_t> completed_frame_slot;
    std::optional<VulkanFrameToken> frame;
};

class VulkanFrameManager {
public:
    VulkanFrameManager(core::Window& window,
                       const VulkanDevice& device,
                       VulkanAllocator& allocator,
                       const VulkanSurface& surface,
                       uint32_t frames_in_flight);
    ~VulkanFrameManager();

    VulkanFrameManager(const VulkanFrameManager&) = delete;
    VulkanFrameManager& operator=(const VulkanFrameManager&) = delete;

    VulkanFrameBeginResult beginFrame();

    void requestSwapchainRecreation() noexcept;
    void waitIdle() const;
    size_t frameSlotCount() const noexcept;
    VulkanPresentationInfo presentationInfo() const noexcept;

private:
    friend class VulkanFrameToken;

    size_t submitAndPresent(VulkanFrameToken& frame);
    void abandonFrame(VulkanFrameToken& frame) noexcept;
    void recoverAbandonedFrame();
    bool recreateSwapchain();
    void createDepthBuffers();
    void createRenderFinishedSemaphores();

    core::Window& m_window;
    const VulkanDevice& m_device;
    VulkanAllocator& m_allocator;
    VulkanSwapchain m_swapchain;
    std::vector<vk::raii::Semaphore> m_render_finished_semaphores;
    std::vector<VulkanFrameSlot> m_frame_slots;
    std::vector<VulkanDepthBuffer> m_depth_buffers;
    std::vector<bool> m_frame_slot_submitted;
    size_t m_current_frame_slot{0};
    bool m_recreate_swapchain{false};
    bool m_frame_active{false};
    bool m_recovery_required{false};
};

} // namespace arti::renderer::vulkan
