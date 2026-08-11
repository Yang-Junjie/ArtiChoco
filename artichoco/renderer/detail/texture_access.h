#pragma once
#include "artichoco/renderer/detail/deferred_resource_owner.h"
#include "artichoco/renderer/texture_2d.h"
#include "artichoco/renderer/texture_cube.h"

#include <cstddef>

#include <span>
#include <vulkan/vulkan.hpp>

namespace arti::renderer::vulkan {
class VulkanAllocator;
class VulkanDevice;
class VulkanImage;
class VulkanUploadContext;
} // namespace arti::renderer::vulkan

namespace arti::renderer::detail {

class TextureAccess {
public:
    static Texture2D create(vulkan::VulkanAllocator& allocator,
            vulkan::VulkanUploadContext& upload_context, const vulkan::VulkanDevice& device,
            DeferredResourceOwnerPtr owner, std::span<const std::byte> texels, uint32_t width,
            uint32_t height, TextureFormat format, bool generate_mipmaps);
    static TextureCube createCube(vulkan::VulkanAllocator& allocator,
            vulkan::VulkanUploadContext& upload_context, const vulkan::VulkanDevice& device,
            DeferredResourceOwnerPtr owner, std::span<const TextureCubeMipData> mip_levels,
            TextureFormat format);

    static const vulkan::VulkanImage& image(const Texture2D& texture) noexcept;
    static const vulkan::VulkanImage& image(const TextureCube& texture) noexcept;
    static bool isOwnedBy(const Texture2D& texture, const DeferredResourceOwner* owner) noexcept;
    static bool isOwnedBy(const TextureCube& texture, const DeferredResourceOwner* owner) noexcept;
};

} // namespace arti::renderer::detail
