#pragma once
#include "vulkan_surface_source.h"

#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

class VulkanSurface {
public:
    VulkanSurface(const vk::raii::Instance& instance, const VulkanSurfaceSource& source);

    VulkanSurface(const VulkanSurface&) = delete;
    VulkanSurface& operator=(const VulkanSurface&) = delete;

    const vk::raii::SurfaceKHR& handle() const noexcept;

private:
    vk::raii::SurfaceKHR m_surface{nullptr};
};

} // namespace arti::renderer::vulkan
