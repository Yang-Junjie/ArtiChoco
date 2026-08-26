#pragma once

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace arti::renderer::detail {

struct NvrhiTextureReadbackData {
    uint32_t width{ 0 };
    uint32_t height{ 0 };
    nvrhi::Format format{ nvrhi::Format::UNKNOWN };
    std::vector<float> rgba;
};

// Internal backend utility. It deliberately operates on NVRHI resources and is not part of the
// RenderDevice or ArtiRenderer public API.
class NvrhiTextureReadback {
public:
    void prepare(nvrhi::IDevice& device, const nvrhi::TextureDesc& source_desc,
            std::string_view debug_name);
    void enqueue(nvrhi::ICommandList& commands, nvrhi::ITexture& source);
    NvrhiTextureReadbackData read(nvrhi::IDevice& device) const;
    bool isReady() const noexcept { return bool(m_staging); }
    void reset() noexcept;

private:
    nvrhi::StagingTextureHandle m_staging;
    uint32_t m_width{ 0 };
    uint32_t m_height{ 0 };
    nvrhi::Format m_format{ nvrhi::Format::UNKNOWN };
};

} // namespace arti::renderer::detail
