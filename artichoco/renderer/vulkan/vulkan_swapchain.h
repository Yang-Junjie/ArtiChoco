#pragma once
#include "artichoco/core/window.h"
#include "vulkan_device.h"
#include "vulkan_surface.h"

#include <nvrhi/nvrhi.h>
#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <vector>

namespace arti::renderer::vulkan {

class NvrhiVulkanDevice;

class VulkanSwapchain {
public:
    VulkanSwapchain(core::Window& window, const VulkanDevice& device,
            NvrhiVulkanDevice& nvrhi_device, const VulkanSurface& surface, bool vsync);

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    bool recreate();
    void invalidate() noexcept;
    bool isRenderable() const noexcept;

    const vk::raii::SwapchainKHR& handle() const noexcept;
    vk::Image image(uint32_t index) const;
    const vk::raii::ImageView& imageView(uint32_t index) const;
    nvrhi::ITexture& nvrhiTexture(uint32_t index) const;
    nvrhi::IFramebuffer& nvrhiFramebuffer(uint32_t index) const;
    vk::Format format() const noexcept;
    vk::Extent2D extent() const noexcept;
    vk::PresentModeKHR presentMode() const noexcept;
    bool vsync() const noexcept;
    void setVsync(bool enabled) noexcept;
    uint32_t minImageCount() const noexcept;
    size_t imageCount() const noexcept;

private:
    core::Window& m_window;
    const VulkanDevice& m_device;
    NvrhiVulkanDevice& m_nvrhi_device;
    const VulkanSurface& m_surface;
    vk::raii::SwapchainKHR m_swapchain{nullptr};
    std::vector<vk::Image> m_images;
    std::vector<vk::raii::ImageView> m_image_views;
    std::vector<nvrhi::TextureHandle> m_nvrhi_textures;
    std::vector<nvrhi::FramebufferHandle> m_nvrhi_framebuffers;
    vk::Format m_format{vk::Format::eUndefined};
    vk::Extent2D m_extent{};
    vk::PresentModeKHR m_present_mode{vk::PresentModeKHR::eFifo};
    bool m_vsync{true};
    uint32_t m_min_image_count{0};
};

} // namespace arti::renderer::vulkan
