#include "texture_cube.h"
#include "detail/texture_format.h"
#include "detail/texture_access.h"
#include "vulkan/vulkan_allocator.h"
#include "vulkan/vulkan_image.h"
#include "vulkan/vulkan_resource_state.h"
#include "vulkan/vulkan_upload_context.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace arti::renderer {

struct TextureCube::Impl {
    vulkan::VulkanImage image;
    detail::DeferredResourceOwnerPtr owner;
    uint32_t size{ 0 };
    uint32_t mip_levels{ 0 };
    TextureFormat format{ TextureFormat::RGBA8Srgb };

    ~Impl() {
        if (owner && image.image()) {
            owner->deferRelease(std::packaged_task<void()>{
                [allocation = std::move(image)]() mutable { (void) allocation; },
            });
        }
    }
};

TextureCube::TextureCube(std::unique_ptr<Impl> impl) noexcept
        : m_impl(std::move(impl)) {}

TextureCube::~TextureCube() = default;
TextureCube::TextureCube(TextureCube&&) noexcept = default;
TextureCube& TextureCube::operator=(TextureCube&&) noexcept = default;

uint32_t TextureCube::size() const noexcept { return m_impl->size; }

uint32_t TextureCube::mipLevels() const noexcept { return m_impl->mip_levels; }

TextureFormat TextureCube::format() const noexcept { return m_impl->format; }

TextureCube detail::TextureAccess::createCube(vulkan::VulkanAllocator& allocator,
        vulkan::VulkanUploadContext& upload_context, const vulkan::VulkanDevice& device,
        DeferredResourceOwnerPtr owner, std::span<const TextureCubeMipData> mip_levels,
        TextureFormat format) {
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

    std::vector<std::byte> pixels;
    pixels.reserve(total_size);
    std::vector<vk::BufferImageCopy> regions;
    regions.reserve(mip_levels.size() * 6);
    for (uint32_t mip_level = 0; mip_level < mip_levels.size(); ++mip_level) {
        const auto& mip = mip_levels[mip_level];
        const auto ordered_faces = mip.faces.ordered();
        for (uint32_t layer = 0; layer < ordered_faces.size(); ++layer) {
            const auto face = ordered_faces[layer];
            const vk::DeviceSize buffer_offset = pixels.size();
            pixels.insert(pixels.end(), face.begin(), face.end());

            vk::ImageSubresourceLayers subresource;
            subresource.setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setMipLevel(mip_level)
                    .setBaseArrayLayer(layer)
                    .setLayerCount(1);
            vk::BufferImageCopy region;
            region.setBufferOffset(buffer_offset)
                    .setBufferRowLength(0)
                    .setBufferImageHeight(0)
                    .setImageSubresource(subresource)
                    .setImageOffset({ 0, 0, 0 })
                    .setImageExtent({ mip.size, mip.size, 1 });
            regions.push_back(region);
        }
    }

    auto impl = std::make_unique<TextureCube::Impl>();
    vulkan::VulkanImageCreateInfo image_info;
    image_info.extent = vk::Extent2D{ base_size, base_size };
    image_info.format = detail::toVulkanFormat(format);
    image_info.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    image_info.mip_levels = static_cast<uint32_t>(mip_levels.size());
    image_info.array_layers = 6;
    image_info.flags = vk::ImageCreateFlagBits::eCubeCompatible;
    image_info.view_type = vk::ImageViewType::eCube;
    impl->image = vulkan::VulkanImage{ device, allocator, image_info };
    impl->owner = std::move(owner);
    impl->size = base_size;
    impl->mip_levels = static_cast<uint32_t>(mip_levels.size());
    impl->format = format;

    vk::ImageSubresourceRange range;
    range.setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setBaseMipLevel(0)
            .setLevelCount(static_cast<uint32_t>(mip_levels.size()))
            .setBaseArrayLayer(0)
            .setLayerCount(6);
    const vulkan::VulkanImageState shader_read{
        vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderSampledRead,
        vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    upload_context.uploadImage(pixels, impl->image.image(), regions, range, shader_read);
    return TextureCube{ std::move(impl) };
}

const vulkan::VulkanImage& detail::TextureAccess::image(const TextureCube& texture) noexcept {
    return texture.m_impl->image;
}

bool detail::TextureAccess::isOwnedBy(const TextureCube& texture,
        const DeferredResourceOwner* owner) noexcept {
    return texture.m_impl->owner.get() == owner;
}

} // namespace arti::renderer
