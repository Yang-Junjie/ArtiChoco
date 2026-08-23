#include "artichoco/renderer/vulkan/vulkan_frame_policy.h"

#include <iostream>
#include <stdexcept>

namespace {

using arti::renderer::vulkan::FramebufferAction;
using arti::renderer::vulkan::FramebufferState;
using arti::renderer::vulkan::SwapchainStatus;
using arti::renderer::vulkan::evaluateFramebufferState;
using arti::renderer::vulkan::requiresSwapchainRecreation;

void expect(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main()
{
    try {
        expect(evaluateFramebufferState({1280, 720, 1280, 720, false}) ==
                        FramebufferAction::Render,
                "A matching framebuffer should render.");
        expect(evaluateFramebufferState({960, 540, 1280, 720, false}) ==
                        FramebufferAction::RecreateSwapchain,
                "A resized framebuffer should recreate the swapchain.");
        expect(evaluateFramebufferState({1280, 720, 1280, 720, true}) ==
                        FramebufferAction::RecreateSwapchain,
                "An explicit recreation request should recreate the swapchain.");
        expect(evaluateFramebufferState({0, 720, 1280, 720, false}) ==
                        FramebufferAction::DeferWhileMinimized,
                "A zero-width framebuffer should defer rendering.");
        expect(evaluateFramebufferState({1280, 0, 1280, 720, true}) ==
                        FramebufferAction::DeferWhileMinimized,
                "A minimized framebuffer should defer before recreation.");

        expect(!requiresSwapchainRecreation(
                        SwapchainStatus::Success, SwapchainStatus::Success),
                "Successful acquire and present should not recreate the swapchain.");
        expect(requiresSwapchainRecreation(
                        SwapchainStatus::Suboptimal, SwapchainStatus::Success),
                "A suboptimal acquire should recreate the swapchain after presenting.");
        expect(requiresSwapchainRecreation(
                        SwapchainStatus::Success, SwapchainStatus::Suboptimal),
                "A suboptimal present should recreate the swapchain.");
        expect(requiresSwapchainRecreation(
                        SwapchainStatus::OutOfDate, SwapchainStatus::Success),
                "An out-of-date acquire should recreate the swapchain.");
        expect(requiresSwapchainRecreation(
                        SwapchainStatus::Success, SwapchainStatus::OutOfDate),
                "An out-of-date present should recreate the swapchain.");

        std::cout << "Vulkan frame lifecycle policy smoke test passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
