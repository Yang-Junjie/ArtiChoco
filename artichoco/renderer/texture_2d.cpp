#include "texture_2d.h"
#include "detail/texture_format.h"
#include "detail/texture_access.h"
#include "vulkan/nvrhi_mipmap.h"
#include "vulkan/nvrhi_resource_upload.h"

#include <nvrhi/nvrhi.h>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace arti::renderer {
namespace {

uint32_t mipLevelCount(uint32_t width, uint32_t height) noexcept
{
    uint32_t levels = 1;
    while (width > 1 || height > 1) {
        width = std::max(1u, width / 2);
        height = std::max(1u, height / 2);
        ++levels;
    }
    return levels;
}

} // namespace

struct Texture2D::Impl {
    detail::ResourceOwnerPtr owner;
    nvrhi::TextureHandle texture;
    uint32_t width{ 0 };
    uint32_t height{ 0 };
    uint32_t mip_levels{ 0 };
    TextureFormat format{ TextureFormat::RGBA8Srgb };

};

Texture2D::Texture2D(std::unique_ptr<Impl> impl) noexcept
        : m_impl(std::move(impl)) {}

Texture2D::~Texture2D() = default;
Texture2D::Texture2D(Texture2D&&) noexcept = default;
Texture2D& Texture2D::operator=(Texture2D&&) noexcept = default;

uint32_t Texture2D::width() const noexcept { return m_impl->width; }

uint32_t Texture2D::height() const noexcept { return m_impl->height; }

uint32_t Texture2D::mipLevels() const noexcept { return m_impl->mip_levels; }

TextureFormat Texture2D::format() const noexcept { return m_impl->format; }

Texture2D detail::TextureAccess::create(nvrhi::IDevice& device,
        ResourceOwnerPtr owner, std::span<const std::byte> texels, uint32_t width,
        uint32_t height, TextureFormat format, bool generate_mipmaps) {
    if (!owner || width == 0 || height == 0) {
        throw std::invalid_argument("A texture requires an owner and non-zero dimensions.");
    }
    const size_t bytes_per_texel = detail::bytesPerTexel(format);
    if (width > std::numeric_limits<size_t>::max() / height / bytes_per_texel ||
            texels.size() != static_cast<size_t>(width) * height * bytes_per_texel) {
        throw std::invalid_argument("Texture data does not match its dimensions and format.");
    }

    const uint32_t mip_levels = generate_mipmaps ? mipLevelCount(width, height) : 1;
    auto impl = std::make_unique<Texture2D::Impl>();
    nvrhi::TextureDesc texture_desc;
    texture_desc.setWidth(width)
            .setHeight(height)
            .setMipLevels(mip_levels)
            .setDimension(nvrhi::TextureDimension::Texture2D)
            .setFormat(detail::toNvrhiFormat(format))
            .setDebugName("ArtiChoco Texture2D");
    if (generate_mipmaps && mip_levels > 1) {
        texture_desc.setIsUAV(true)
                .setIsTypeless(true)
                .enableAutomaticStateTracking(nvrhi::ResourceStates::CopyDest);
        impl->texture = device.createTexture(texture_desc);
        if (!impl->texture) {
            throw std::runtime_error("NVRHI failed to create a mipmapped texture.");
        }
        vulkan::uploadAndGenerateNvrhiTextureMipmaps(device, impl->texture, texels,
                static_cast<size_t>(width) * bytes_per_texel,
                detail::toNvrhiFormat(format), detail::toNvrhiStorageFormat(format));
    } else {
        const std::array uploads = {vulkan::NvrhiTextureUpload{
                0, 0, texels, static_cast<size_t>(width) * bytes_per_texel, 0}};
        impl->texture = vulkan::createAndUploadNvrhiTexture(device, std::move(texture_desc),
                uploads, nvrhi::ResourceStates::ShaderResource);
    }
    impl->owner = std::move(owner);
    impl->width = width;
    impl->height = height;
    impl->mip_levels = mip_levels;
    impl->format = format;
    return Texture2D{ std::move(impl) };
}

nvrhi::ITexture& detail::TextureAccess::nvrhiHandle(const Texture2D& texture) noexcept {
    return *texture.m_impl->texture;
}

bool detail::TextureAccess::isOwnedBy(const Texture2D& texture,
        const ResourceOwner* owner) noexcept {
    return texture.m_impl->owner.get() == owner;
}

} // namespace arti::renderer
