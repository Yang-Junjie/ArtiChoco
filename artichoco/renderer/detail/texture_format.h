#pragma once
#include "artichoco/renderer/texture_format.h"

#include <cstddef>
#include <stdexcept>
#include <vulkan/vulkan.hpp>

namespace arti::renderer::detail {

inline vk::Format toVulkanFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGBA8Unorm:
            return vk::Format::eR8G8B8A8Unorm;
        case TextureFormat::RGBA8Srgb:
            return vk::Format::eR8G8B8A8Srgb;
        case TextureFormat::RGBA16Float:
            return vk::Format::eR16G16B16A16Sfloat;
    }
    throw std::invalid_argument("Unsupported texture format.");
}

inline size_t bytesPerTexel(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGBA8Unorm:
        case TextureFormat::RGBA8Srgb:
            return 4;
        case TextureFormat::RGBA16Float:
            return 8;
    }
    throw std::invalid_argument("Unsupported texture format.");
}

} // namespace arti::renderer::detail
