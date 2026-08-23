#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "nvrhi_vulkan_dispatch.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace arti::renderer::vulkan::detail {

void initializeNvrhiVulkanDispatcher(VkInstance instance, VkDevice device)
{
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance, vkGetInstanceProcAddr, device);
}

} // namespace arti::renderer::vulkan::detail
