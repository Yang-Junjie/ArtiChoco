#pragma once
#include "artichoco/renderer/texture_format.h"

#include <cstddef>
#include <nvrhi/nvrhi.h>
#include <stdexcept>

namespace arti::renderer::detail {

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

inline nvrhi::Format toNvrhiFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGBA8Unorm:
            return nvrhi::Format::RGBA8_UNORM;
        case TextureFormat::RGBA8Srgb:
            return nvrhi::Format::SRGBA8_UNORM;
        case TextureFormat::RGBA16Float:
            return nvrhi::Format::RGBA16_FLOAT;
    }
    throw std::invalid_argument("Unsupported texture format.");
}

inline nvrhi::Format toNvrhiStorageFormat(TextureFormat format) {
    return format == TextureFormat::RGBA8Srgb
            ? nvrhi::Format::RGBA8_UNORM
            : toNvrhiFormat(format);
}

} // namespace arti::renderer::detail
