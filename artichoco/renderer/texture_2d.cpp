#include "texture_2d.h"

#include "texture_access.h"
#include "vulkan/vulkan_allocator.h"
#include "vulkan/vulkan_texture_descriptors.h"
#include "vulkan/vulkan_upload_context.h"

#include <vulkan/vulkan_raii.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

namespace arti::renderer {
namespace {

vk::Format toVulkanFormat(TextureFormat format)
{
    switch (format) {
    case TextureFormat::RGBA8Unorm:
        return vk::Format::eR8G8B8A8Unorm;
    case TextureFormat::RGBA8Srgb:
        return vk::Format::eR8G8B8A8Srgb;
    }
    throw std::invalid_argument("Unsupported texture format.");
}

struct TextureResource {
    vulkan::AllocatedImage image;
    vk::raii::ImageView image_view{nullptr};
    vk::raii::Sampler sampler{nullptr};
    vk::raii::DescriptorSet descriptor_set{nullptr};

    TextureResource() = default;
    TextureResource(const TextureResource&) = delete;
    TextureResource& operator=(const TextureResource&) = delete;
    TextureResource(TextureResource&&) noexcept = default;
    TextureResource& operator=(TextureResource&&) noexcept = default;
};

} // namespace

struct Texture2D::Impl {
    TextureResource resource;
    detail::DeferredResourceOwnerPtr owner;
    uint32_t width{0};
    uint32_t height{0};
    TextureFormat format{TextureFormat::RGBA8Srgb};

    ~Impl()
    {
        if (owner && resource.image.handle()) {
            owner->deferRelease(std::packaged_task<void()>{
                [allocation = std::move(resource)]() mutable { (void)allocation; },
            });
        }
    }
};

Texture2D::Texture2D(std::unique_ptr<Impl> impl) noexcept
    : m_impl(std::move(impl))
{}

Texture2D::~Texture2D() = default;
Texture2D::Texture2D(Texture2D&&) noexcept = default;
Texture2D& Texture2D::operator=(Texture2D&&) noexcept = default;

uint32_t Texture2D::width() const noexcept
{
    return m_impl->width;
}

uint32_t Texture2D::height() const noexcept
{
    return m_impl->height;
}

TextureFormat Texture2D::format() const noexcept
{
    return m_impl->format;
}

Texture2D detail::TextureAccess::create(
    vulkan::VulkanAllocator& allocator,
    vulkan::VulkanUploadContext& upload_context,
    vulkan::VulkanTextureDescriptors& descriptors,
    DeferredResourceOwnerPtr owner,
    std::span<const std::byte> rgba_pixels,
    uint32_t width,
    uint32_t height,
    TextureFormat format)
{
    if (!owner || width == 0 || height == 0) {
        throw std::invalid_argument("A texture requires an owner and non-zero dimensions.");
    }
    if (width > std::numeric_limits<size_t>::max() / height / 4 ||
        rgba_pixels.size() != static_cast<size_t>(width) * height * 4) {
        throw std::invalid_argument("RGBA texture data does not match its dimensions.");
    }

    const vk::Format vulkan_format = toVulkanFormat(format);
    vk::ImageCreateInfo image_info{};
    image_info.setImageType(vk::ImageType::e2D)
        .setFormat(vulkan_format)
        .setExtent({width, height, 1})
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);
    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    auto impl = std::make_unique<Texture2D::Impl>();
    impl->resource.image = allocator.createImage(image_info, allocation_info);
    impl->owner = std::move(owner);
    impl->width = width;
    impl->height = height;
    impl->format = format;
    upload_context.uploadImageRGBA8(rgba_pixels, impl->resource.image.handle(), {width, height});

    vk::ImageSubresourceRange range{};
    range.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    vk::ImageViewCreateInfo view_info{};
    view_info.setImage(impl->resource.image.handle())
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(vulkan_format)
        .setSubresourceRange(range);
    impl->resource.image_view = vk::raii::ImageView{descriptors.device(), view_info};

    vk::SamplerCreateInfo sampler_info{};
    sampler_info.setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setAddressModeU(vk::SamplerAddressMode::eRepeat)
        .setAddressModeV(vk::SamplerAddressMode::eRepeat)
        .setAddressModeW(vk::SamplerAddressMode::eRepeat)
        .setMipLodBias(0.0f)
        .setAnisotropyEnable(false)
        .setCompareEnable(false)
        .setMinLod(0.0f)
        .setMaxLod(0.0f)
        .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
        .setUnnormalizedCoordinates(false);
    impl->resource.sampler = vk::raii::Sampler{descriptors.device(), sampler_info};
    impl->resource.descriptor_set = descriptors.createTextureSet(
        impl->resource.image_view, impl->resource.sampler);
    return Texture2D{std::move(impl)};
}

vk::DescriptorSet detail::TextureAccess::descriptorSet(const Texture2D& texture) noexcept
{
    return *texture.m_impl->resource.descriptor_set;
}

bool detail::TextureAccess::isOwnedBy(const Texture2D& texture, const DeferredResourceOwner* owner) noexcept
{
    return texture.m_impl->owner.get() == owner;
}

} // namespace arti::renderer
