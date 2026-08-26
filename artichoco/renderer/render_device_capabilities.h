#pragma once

#include <cstdint>

namespace arti::renderer {

enum class RenderDeviceFormat {
    RGBA8Unorm,
    RGBA8Srgb,
    RGBA16Float,
    D32Float,
};

struct RenderFormatSupport {
    bool texture{ false };
    bool shader_sample{ false };
    bool render_target{ false };
    bool depth_stencil{ false };
    uint32_t sample_count_mask{ 0 };

    bool supportsSampleCount(uint32_t sample_count) const noexcept {
        return sample_count != 0 && (sample_count & (sample_count - 1U)) == 0 &&
               (sample_count_mask & sample_count) != 0;
    }
};

} // namespace arti::renderer
