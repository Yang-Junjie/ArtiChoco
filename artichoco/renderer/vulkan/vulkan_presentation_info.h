#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>

namespace arti::renderer::vulkan {

struct VulkanPresentationInfo {
    vk::Format color_format{vk::Format::eUndefined};
    uint32_t min_image_count{0};
    uint32_t image_count{0};
};

} // namespace arti::renderer::vulkan
