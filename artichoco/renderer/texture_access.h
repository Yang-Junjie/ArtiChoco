#pragma once
#include "detail/deferred_resource_owner.h"
#include "texture_2d.h"

#include <vulkan/vulkan.hpp>

#include <cstddef>
#include <span>

namespace arti::renderer::vulkan {
class VulkanAllocator;
class VulkanTextureDescriptors;
class VulkanUploadContext;
}

namespace arti::renderer::detail {

class TextureAccess {
public:
    static Texture2D create(
        vulkan::VulkanAllocator& allocator,
        vulkan::VulkanUploadContext& upload_context,
        vulkan::VulkanTextureDescriptors& descriptors,
        DeferredResourceOwnerPtr owner,
        std::span<const std::byte> rgba_pixels,
        uint32_t width,
        uint32_t height,
        TextureFormat format);

    static vk::DescriptorSet descriptorSet(const Texture2D& texture) noexcept;
    static bool isOwnedBy(const Texture2D& texture, const DeferredResourceOwner* owner) noexcept;
};

} // namespace arti::renderer::detail
