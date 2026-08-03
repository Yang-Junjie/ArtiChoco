#include "vulkan_surface.h"

#include "artichoco/renderer/renderer_log.h"

#include <stdexcept>

namespace arti::renderer::vulkan {

VulkanSurface::VulkanSurface(const vk::raii::Instance& instance, const VulkanSurfaceSource& source)
{
    VkSurfaceKHR raw_surface = VK_NULL_HANDLE;
    const VkResult result = source.createSurface(static_cast<VkInstance>(*instance), nullptr, &raw_surface);
    if (result != VK_SUCCESS || raw_surface == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to create Vulkan presentation surface.");
    }

    m_surface = vk::raii::SurfaceKHR{instance, vk::SurfaceKHR{raw_surface}};
    getLogChannel().info("Created Vulkan presentation surface");
}

const vk::raii::SurfaceKHR& VulkanSurface::handle() const noexcept
{
    return m_surface;
}

} // namespace arti::renderer::vulkan
