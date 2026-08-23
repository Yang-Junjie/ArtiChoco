#pragma once

#include <vulkan/vulkan.h>

namespace arti::renderer::vulkan::detail {

void initializeNvrhiVulkanDispatcher(VkInstance instance, VkDevice device);

} // namespace arti::renderer::vulkan::detail
