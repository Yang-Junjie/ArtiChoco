#pragma once
#include "artichoco/renderer/detail/resource_owner.h"
#include "artichoco/renderer/texture_2d.h"
#include "artichoco/renderer/texture_cube.h"

#include <cstddef>

#include <span>

namespace nvrhi {
class IDevice;
class ITexture;
} // namespace nvrhi

namespace arti::renderer::detail {

class TextureAccess {
public:
    static Texture2D create(nvrhi::IDevice& device,
            ResourceOwnerPtr owner, std::span<const std::byte> texels, uint32_t width,
            uint32_t height, TextureFormat format, bool generate_mipmaps);
    static TextureCube createCube(nvrhi::IDevice& device,
            ResourceOwnerPtr owner, std::span<const TextureCubeMipData> mip_levels,
            TextureFormat format);

    static nvrhi::ITexture& nvrhiHandle(const Texture2D& texture) noexcept;
    static nvrhi::ITexture& nvrhiHandle(const TextureCube& texture) noexcept;
    static bool isOwnedBy(const Texture2D& texture, const ResourceOwner* owner) noexcept;
    static bool isOwnedBy(const TextureCube& texture, const ResourceOwner* owner) noexcept;
};

} // namespace arti::renderer::detail
