#pragma once
#include "vulkan_binding_layout.h"

#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

vk::raii::PipelineLayout createPipelineLayout(const VulkanDevice& device, const VulkanBindingLayout& binding_layout);

} // namespace arti::renderer::vulkan
