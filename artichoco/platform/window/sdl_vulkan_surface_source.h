#pragma once
#include "artichoco/core/window.h"
#include "artichoco/renderer/vulkan/vulkan_surface_source.h"

#include <memory>

namespace arti::platform {

std::unique_ptr<renderer::vulkan::VulkanSurfaceSource> createSDLVulkanSurfaceSource(core::Window& window);

} // namespace arti::platform
