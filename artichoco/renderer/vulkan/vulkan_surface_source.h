#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace arti::renderer::vulkan {

class VulkanSurfaceSource {
public:
    virtual ~VulkanSurfaceSource() = default;

    virtual std::vector<std::string> requiredInstanceExtensions() const = 0;
    virtual VkResult
        createSurface(VkInstance instance, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface) const = 0;
};

} // namespace arti::renderer::vulkan
