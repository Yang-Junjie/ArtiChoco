#pragma once
#include "artichoco/core/window.h"
#include "vulkan_device.h"
#include "vulkan_surface.h"

#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <vector>

namespace arti::renderer::vulkan {

class VulkanSwapchain {
public:
    VulkanSwapchain(core::Window& window, const VulkanDevice& device, const VulkanSurface& surface);

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    bool recreate();
    void invalidate() noexcept;
    bool isRenderable() const noexcept;

    const vk::raii::SwapchainKHR& handle() const noexcept;
    vk::Image image(uint32_t index) const;
    const vk::raii::ImageView& imageView(uint32_t index) const;
    vk::Format format() const noexcept;
    vk::Extent2D extent() const noexcept;
    size_t imageCount() const noexcept;

private:
    core::Window& m_window;
    const VulkanDevice& m_device;
    const VulkanSurface& m_surface;
    vk::raii::SwapchainKHR m_swapchain{nullptr};
    std::vector<vk::Image> m_images;
    std::vector<vk::raii::ImageView> m_image_views;
    vk::Format m_format{vk::Format::eUndefined};
    vk::Extent2D m_extent{};
};

} // namespace arti::renderer::vulkan
