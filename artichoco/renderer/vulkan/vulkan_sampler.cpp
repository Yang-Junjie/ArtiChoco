#include "vulkan_sampler.h"

#include <algorithm>
#include <stdexcept>

namespace arti::renderer::vulkan {

VulkanSampler::VulkanSampler(const VulkanDevice& device, const VulkanSamplerCreateInfo& info)
{
    float max_anisotropy = info.max_anisotropy;
    if (info.anisotropy_enable) {
        if (!device.samplerAnisotropyEnabled()) {
            throw std::invalid_argument(
                "Anisotropic filtering requires the samplerAnisotropy device feature, which is not enabled.");
        }
        max_anisotropy = std::min(
            max_anisotropy, device.physicalDevice().getProperties().limits.maxSamplerAnisotropy);
    }

    vk::SamplerCreateInfo sampler_info{};
    sampler_info.setMagFilter(info.mag_filter)
        .setMinFilter(info.min_filter)
        .setMipmapMode(info.mipmap_mode)
        .setAddressModeU(info.address_mode_u)
        .setAddressModeV(info.address_mode_v)
        .setAddressModeW(info.address_mode_w)
        .setMipLodBias(info.mip_lod_bias)
        .setAnisotropyEnable(info.anisotropy_enable)
        .setMaxAnisotropy(max_anisotropy)
        .setCompareEnable(info.compare_enable)
        .setCompareOp(info.compare_op)
        .setMinLod(info.min_lod)
        .setMaxLod(info.max_lod)
        .setBorderColor(info.border_color)
        .setUnnormalizedCoordinates(info.unnormalized_coordinates)
        .setFlags(info.flags);
    m_sampler = vk::raii::Sampler{device.device(), sampler_info};
}

const vk::raii::Sampler& VulkanSampler::handle() const noexcept
{
    return m_sampler;
}

} // namespace arti::renderer::vulkan
