#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace arti::renderer::vulkan {

class VulkanDevice {
public:
    VulkanDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface);

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    const vk::raii::PhysicalDevice& physicalDevice() const noexcept;
    const vk::raii::Device& device() const noexcept;
    const vk::raii::Queue& graphicsQueue() const noexcept;
    const vk::raii::Queue& presentQueue() const noexcept;
    uint32_t graphicsQueueFamily() const noexcept;
    uint32_t presentQueueFamily() const noexcept;
    std::span<const char* const> enabledExtensions() const noexcept;
    bool usesCore13() const noexcept;
    bool mutableSwapchainFormatEnabled() const noexcept;
    bool independentBlendEnabled() const noexcept;
    bool samplerAnisotropyEnabled() const noexcept;
    bool descriptorIndexingEnabled() const noexcept;

private:
    vk::raii::PhysicalDevice m_physical_device{nullptr};
    vk::raii::Device m_device{nullptr};
    vk::raii::Queue m_graphics_queue{nullptr};
    vk::raii::Queue m_present_queue{nullptr};
    uint32_t m_graphics_queue_family{0};
    uint32_t m_present_queue_family{0};
    std::vector<const char*> m_enabled_extensions;
    bool m_uses_core_13{false};
    bool m_mutable_swapchain_format_enabled{false};
    bool m_independent_blend_enabled{false};
    bool m_sampler_anisotropy_enabled{false};
    bool m_descriptor_indexing_enabled{false};
};

} // namespace arti::renderer::vulkan
