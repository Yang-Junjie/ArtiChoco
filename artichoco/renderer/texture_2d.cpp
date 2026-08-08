#include "texture_2d.h"
#include "detail/texture_format.h"
#include "detail/texture_access.h"
#include "vulkan/vulkan_allocator.h"
#include "vulkan/vulkan_image.h"
#include "vulkan/vulkan_resource_state.h"
#include "vulkan/vulkan_upload_context.h"

#include <limits>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer {

struct Texture2D::Impl {
    vulkan::VulkanImage image;
    detail::DeferredResourceOwnerPtr owner;
    uint32_t width{ 0 };
    uint32_t height{ 0 };
    TextureFormat format{ TextureFormat::RGBA8Srgb };

    ~Impl() {
        if (owner && image.image()) {
            owner->deferRelease(std::packaged_task<void()>{
                [allocation = std::move(image)]() mutable { (void) allocation; },
            });
        }
    }
};

Texture2D::Texture2D(std::unique_ptr<Impl> impl) noexcept
        : m_impl(std::move(impl)) {}

Texture2D::~Texture2D() = default;
Texture2D::Texture2D(Texture2D&&) noexcept = default;
Texture2D& Texture2D::operator=(Texture2D&&) noexcept = default;

uint32_t Texture2D::width() const noexcept { return m_impl->width; }

uint32_t Texture2D::height() const noexcept { return m_impl->height; }

TextureFormat Texture2D::format() const noexcept { return m_impl->format; }

Texture2D detail::TextureAccess::create(vulkan::VulkanAllocator& allocator,
        vulkan::VulkanUploadContext& upload_context, const vulkan::VulkanDevice& device,
        DeferredResourceOwnerPtr owner, std::span<const std::byte> texels, uint32_t width,
        uint32_t height, TextureFormat format) {
    if (!owner || width == 0 || height == 0) {
        throw std::invalid_argument("A texture requires an owner and non-zero dimensions.");
    }
    const size_t bytes_per_texel = detail::bytesPerTexel(format);
    if (width > std::numeric_limits<size_t>::max() / height / bytes_per_texel ||
            texels.size() != static_cast<size_t>(width) * height * bytes_per_texel) {
        throw std::invalid_argument("Texture data does not match its dimensions and format.");
    }

    const vk::Format vulkan_format = detail::toVulkanFormat(format);
    auto impl = std::make_unique<Texture2D::Impl>();
    vulkan::VulkanImageCreateInfo image_info;
    image_info.extent = vk::Extent2D{ width, height };
    image_info.format = vulkan_format;
    image_info.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    impl->image = vulkan::VulkanImage{ device, allocator, image_info };
    impl->owner = std::move(owner);
    impl->width = width;
    impl->height = height;
    impl->format = format;
    const vulkan::VulkanImageState shader_read{
        vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderSampledRead,
        vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    vk::ImageSubresourceLayers layers;
    layers.setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setMipLevel(0)
            .setBaseArrayLayer(0)
            .setLayerCount(1);
    vk::BufferImageCopy copy;
    copy.setBufferOffset(0)
            .setBufferRowLength(0)
            .setBufferImageHeight(0)
            .setImageSubresource(layers)
            .setImageOffset({ 0, 0, 0 })
            .setImageExtent({ width, height, 1 });
    vk::ImageSubresourceRange range;
    range.setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setBaseMipLevel(0)
            .setLevelCount(1)
            .setBaseArrayLayer(0)
            .setLayerCount(1);
    upload_context.uploadImage(texels, impl->image.image(),
            std::span<const vk::BufferImageCopy>{ &copy, 1 }, range, shader_read);
    return Texture2D{ std::move(impl) };
}

const vulkan::VulkanImage& detail::TextureAccess::image(const Texture2D& texture) noexcept {
    return texture.m_impl->image;
}

bool detail::TextureAccess::isOwnedBy(const Texture2D& texture,
        const DeferredResourceOwner* owner) noexcept {
    return texture.m_impl->owner.get() == owner;
}

} // namespace arti::renderer
