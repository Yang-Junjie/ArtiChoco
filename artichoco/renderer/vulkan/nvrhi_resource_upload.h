#pragma once

#include <nvrhi/nvrhi.h>

#include <cstddef>
#include <span>

namespace arti::renderer::vulkan {

nvrhi::BufferHandle createAndUploadNvrhiBuffer(nvrhi::IDevice& device,
        nvrhi::BufferDesc desc, std::span<const std::byte> data,
        nvrhi::ResourceStates final_state);

struct NvrhiTextureUpload {
    uint32_t array_slice{0};
    uint32_t mip_level{0};
    std::span<const std::byte> data;
    size_t row_pitch{0};
    size_t depth_pitch{0};
};

nvrhi::TextureHandle createAndUploadNvrhiTexture(nvrhi::IDevice& device,
        nvrhi::TextureDesc desc, std::span<const NvrhiTextureUpload> uploads,
        nvrhi::ResourceStates final_state);

} // namespace arti::renderer::vulkan
