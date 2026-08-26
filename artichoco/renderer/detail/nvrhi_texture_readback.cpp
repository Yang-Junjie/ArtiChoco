#include "nvrhi_texture_readback.h"

#include <cstring>
#include <stdexcept>
#include <string>

namespace arti::renderer::detail {
namespace {

float decodeHalf(uint16_t value) noexcept {
    const uint32_t sign = (value >> 15u) & 1u;
    const uint32_t exponent = (value >> 10u) & 0x1fu;
    const uint32_t mantissa = value & 0x3ffu;
    uint32_t result = 0;
    if (exponent == 0) {
        if (mantissa == 0) {
            result = sign << 31u;
        } else {
            uint32_t normalized = mantissa;
            uint32_t shift = 0;
            while ((normalized & 0x400u) == 0) {
                normalized <<= 1u;
                ++shift;
            }
            result = (sign << 31u) | ((113u - shift) << 23u) |
                     ((normalized & 0x3ffu) << 13u);
        }
    } else if (exponent == 0x1fu) {
        result = (sign << 31u) | 0x7f800000u | (mantissa << 13u);
    } else {
        result = (sign << 31u) | ((exponent + 112u) << 23u) | (mantissa << 13u);
    }
    float decoded = 0.0f;
    std::memcpy(&decoded, &result, sizeof(decoded));
    return decoded;
}

bool isRgba8(nvrhi::Format format) noexcept {
    return format == nvrhi::Format::RGBA8_UNORM || format == nvrhi::Format::SRGBA8_UNORM ||
           format == nvrhi::Format::BGRA8_UNORM || format == nvrhi::Format::SBGRA8_UNORM;
}

} // namespace

void NvrhiTextureReadback::prepare(nvrhi::IDevice& device,
        const nvrhi::TextureDesc& source_desc, std::string_view debug_name) {
    if (source_desc.width == 0 || source_desc.height == 0 ||
            (!isRgba8(source_desc.format) && source_desc.format != nvrhi::Format::RGBA16_FLOAT)) {
        throw std::invalid_argument(
                "NVRHI texture readback requires a supported non-zero 2D color texture.");
    }
    if (m_staging && m_width == source_desc.width && m_height == source_desc.height &&
            m_format == source_desc.format) {
        return;
    }
    nvrhi::TextureDesc staging_desc;
    staging_desc.setWidth(source_desc.width)
            .setHeight(source_desc.height)
            .setFormat(source_desc.format)
            .setDebugName(debug_name.empty() ? "ArtiChoco Texture Readback"
                                             : std::string{ debug_name });
    m_staging = device.createStagingTexture(staging_desc, nvrhi::CpuAccessMode::Read);
    if (!m_staging) {
        throw std::runtime_error("NVRHI failed to create a texture readback resource.");
    }
    m_width = source_desc.width;
    m_height = source_desc.height;
    m_format = source_desc.format;
}

void NvrhiTextureReadback::enqueue(nvrhi::ICommandList& commands, nvrhi::ITexture& source) {
    if (!m_staging) {
        throw std::logic_error("NVRHI texture readback was not prepared.");
    }
    commands.copyTexture(m_staging, nvrhi::TextureSlice{}, &source, nvrhi::TextureSlice{});
}

NvrhiTextureReadbackData NvrhiTextureReadback::read(nvrhi::IDevice& device) const {
    if (!m_staging) {
        throw std::logic_error("NVRHI texture readback was not prepared.");
    }
    size_t row_pitch = 0;
    const auto* bytes = static_cast<const uint8_t*>(device.mapStagingTexture(m_staging,
            nvrhi::TextureSlice{}, nvrhi::CpuAccessMode::Read, &row_pitch));
    if (bytes == nullptr) {
        throw std::runtime_error("NVRHI failed to map a texture readback resource.");
    }
    struct UnmapGuard {
        nvrhi::IDevice& device;
        nvrhi::IStagingTexture* texture;
        ~UnmapGuard() { device.unmapStagingTexture(texture); }
    } guard{ device, m_staging };

    const size_t bytes_per_pixel = m_format == nvrhi::Format::RGBA16_FLOAT ? 8u : 4u;
    if (row_pitch < static_cast<size_t>(m_width) * bytes_per_pixel) {
        throw std::runtime_error("NVRHI returned an invalid texture readback row pitch.");
    }
    NvrhiTextureReadbackData result;
    result.width = m_width;
    result.height = m_height;
    result.format = m_format;
    result.rgba.resize(static_cast<size_t>(m_width) * m_height * 4u);
    for (uint32_t y = 0; y < m_height; ++y) {
        const uint8_t* row = bytes + static_cast<size_t>(y) * row_pitch;
        for (uint32_t x = 0; x < m_width; ++x) {
            const uint8_t* pixel = row + static_cast<size_t>(x) * bytes_per_pixel;
            float* destination = result.rgba.data() +
                                 (static_cast<size_t>(y) * m_width + x) * 4u;
            if (m_format == nvrhi::Format::RGBA16_FLOAT) {
                uint16_t half[4];
                std::memcpy(half, pixel, sizeof(half));
                destination[0] = decodeHalf(half[0]);
                destination[1] = decodeHalf(half[1]);
                destination[2] = decodeHalf(half[2]);
                destination[3] = decodeHalf(half[3]);
            } else {
                destination[0] = static_cast<float>(pixel[0]) / 255.0f;
                destination[1] = static_cast<float>(pixel[1]) / 255.0f;
                destination[2] = static_cast<float>(pixel[2]) / 255.0f;
                destination[3] = static_cast<float>(pixel[3]) / 255.0f;
                if (m_format == nvrhi::Format::BGRA8_UNORM ||
                        m_format == nvrhi::Format::SBGRA8_UNORM) {
                    const float red = destination[0];
                    destination[0] = destination[2];
                    destination[2] = red;
                }
            }
        }
    }
    return result;
}

void NvrhiTextureReadback::reset() noexcept {
    m_staging = nullptr;
    m_width = 0;
    m_height = 0;
    m_format = nvrhi::Format::UNKNOWN;
}

} // namespace arti::renderer::detail
