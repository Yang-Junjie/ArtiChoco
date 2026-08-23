#pragma once

#include <nvrhi/nvrhi.h>

#include <cstddef>
#include <cstdint>
#include <span>

namespace arti::renderer::vulkan {

void uploadAndGenerateNvrhiTextureMipmaps(nvrhi::IDevice& device,
        nvrhi::TextureHandle texture, std::span<const std::byte> base_level,
        size_t row_pitch, nvrhi::Format source_view_format,
        nvrhi::Format storage_view_format,
        nvrhi::ResourceStates final_state = nvrhi::ResourceStates::ShaderResource);

} // namespace arti::renderer::vulkan
