#pragma once

#include <cstdint>

namespace arti::renderer::vulkan {

enum class FramebufferAction {
    Render,
    RecreateSwapchain,
    DeferWhileMinimized,
};

struct FramebufferState {
    uint32_t framebuffer_width{0};
    uint32_t framebuffer_height{0};
    uint32_t swapchain_width{0};
    uint32_t swapchain_height{0};
    bool recreation_requested{false};
};

[[nodiscard]] constexpr FramebufferAction evaluateFramebufferState(
        const FramebufferState& state) noexcept
{
    if (state.framebuffer_width == 0 || state.framebuffer_height == 0) {
        return FramebufferAction::DeferWhileMinimized;
    }
    if (state.recreation_requested ||
            state.framebuffer_width != state.swapchain_width ||
            state.framebuffer_height != state.swapchain_height) {
        return FramebufferAction::RecreateSwapchain;
    }
    return FramebufferAction::Render;
}

enum class SwapchainStatus {
    Success,
    Suboptimal,
    OutOfDate,
};

[[nodiscard]] constexpr bool requiresSwapchainRecreation(
        SwapchainStatus acquire_status, SwapchainStatus present_status) noexcept
{
    return acquire_status != SwapchainStatus::Success ||
            present_status != SwapchainStatus::Success;
}

} // namespace arti::renderer::vulkan
