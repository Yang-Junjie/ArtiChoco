#include "texture_cube.h"
#include "detail/texture_access.h"
#include "detail/texture_format.h"
#include "vulkan/nvrhi_resource_upload.h"

#include <nvrhi/nvrhi.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace arti::renderer {

struct TextureCube::Impl {
    detail::ResourceOwnerPtr owner;
    nvrhi::TextureHandle texture;
    uint32_t size{ 0 };
    uint32_t mip_levels{ 0 };
    TextureFormat format{ TextureFormat::RGBA8Srgb };
};

TextureCube::TextureCube(std::unique_ptr<Impl> impl) noexcept
        : m_impl(std::move(impl)) {}

TextureCube::~TextureCube() = default;
TextureCube::TextureCube(TextureCube&&) noexcept = default;
TextureCube& TextureCube::operator=(TextureCube&&) noexcept = default;

uint32_t TextureCube::size() const noexcept { return m_impl->size; }

uint32_t TextureCube::mipLevels() const noexcept { return m_impl->mip_levels; }

TextureFormat TextureCube::format() const noexcept { return m_impl->format; }

TextureCube detail::TextureAccess::createCube(nvrhi::IDevice& device, ResourceOwnerPtr owner,
        std::span<const TextureCubeMipData> mip_levels, TextureFormat format,
        std::string_view debug_name) {
    if (!owner || mip_levels.empty() || mip_levels.front().size == 0) {
        throw std::invalid_argument(
                "A cube texture requires an owner and at least one non-zero mip level.");
    }

    const uint32_t base_size = mip_levels.front().size;
    uint32_t maximum_mip_levels = 1;
    for (uint32_t size = base_size; size > 1; size /= 2) {
        ++maximum_mip_levels;
    }
    if (mip_levels.size() > maximum_mip_levels) {
        throw std::invalid_argument(
                "A cube texture has more mip levels than its base size supports.");
    }

    const size_t bytes_per_texel = detail::bytesPerTexel(format);
    size_t total_size = 0;
    uint32_t expected_size = base_size;
    for (const auto& mip: mip_levels) {
        if (mip.size != expected_size ||
                mip.size > std::numeric_limits<size_t>::max() / mip.size / bytes_per_texel) {
            throw std::invalid_argument("Cube texture mip sizes must halve from the base level.");
        }

        const size_t face_size = static_cast<size_t>(mip.size) * mip.size * bytes_per_texel;
        for (const auto face: mip.faces.ordered()) {
            if (face.size() != face_size) {
                throw std::invalid_argument(
                        "Every cube texture face must match its mip size and texture format.");
            }
        }
        if (face_size > std::numeric_limits<size_t>::max() / 6 ||
                total_size > std::numeric_limits<size_t>::max() - face_size * 6) {
            throw std::invalid_argument("The cube texture data size exceeds addressable memory.");
        }
        total_size += face_size * 6;
        expected_size = std::max(1u, expected_size / 2);
    }

    std::vector<vulkan::NvrhiTextureUpload> uploads;
    uploads.reserve(mip_levels.size() * 6);
    for (uint32_t mip_level = 0; mip_level < mip_levels.size(); ++mip_level) {
        const auto& mip = mip_levels[mip_level];
        const auto ordered_faces = mip.faces.ordered();
        for (uint32_t layer = 0; layer < ordered_faces.size(); ++layer) {
            const auto face = ordered_faces[layer];
            uploads.push_back(
                    { layer, mip_level, face, static_cast<size_t>(mip.size) * bytes_per_texel, 0 });
        }
    }

    auto impl = std::make_unique<TextureCube::Impl>();
    nvrhi::TextureDesc texture_desc;
    texture_desc.setWidth(base_size)
            .setHeight(base_size)
            .setArraySize(6)
            .setMipLevels(static_cast<uint32_t>(mip_levels.size()))
            .setDimension(nvrhi::TextureDimension::TextureCube)
            .setFormat(detail::toNvrhiFormat(format))
            .setDebugName(debug_name.empty() ? "ArtiChoco TextureCube" : std::string{ debug_name });
    impl->texture = vulkan::createAndUploadNvrhiTexture(device, std::move(texture_desc), uploads,
            nvrhi::ResourceStates::ShaderResource);
    impl->owner = std::move(owner);
    impl->size = base_size;
    impl->mip_levels = static_cast<uint32_t>(mip_levels.size());
    impl->format = format;

    return TextureCube{ std::move(impl) };
}

nvrhi::ITexture& detail::TextureAccess::nvrhiHandle(const TextureCube& texture) noexcept {
    return *texture.m_impl->texture;
}

bool detail::TextureAccess::isOwnedBy(const TextureCube& texture,
        const ResourceOwner* owner) noexcept {
    return texture.m_impl->owner.get() == owner;
}

} // namespace arti::renderer
