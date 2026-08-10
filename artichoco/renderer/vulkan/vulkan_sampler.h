#pragma once
#include "vulkan_device.h"

#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

struct VulkanSamplerCreateInfo {
    vk::Filter mag_filter{vk::Filter::eLinear};
    vk::Filter min_filter{vk::Filter::eLinear};
    vk::SamplerMipmapMode mipmap_mode{vk::SamplerMipmapMode::eLinear};
    vk::SamplerAddressMode address_mode_u{vk::SamplerAddressMode::eRepeat};
    vk::SamplerAddressMode address_mode_v{vk::SamplerAddressMode::eRepeat};
    vk::SamplerAddressMode address_mode_w{vk::SamplerAddressMode::eRepeat};
    float mip_lod_bias{0.0f};
    bool anisotropy_enable{true};
    float max_anisotropy{8.0f};
    bool compare_enable{false};
    vk::CompareOp compare_op{vk::CompareOp::eNever};
    float min_lod{0.0f};
    float max_lod{1000.0f};
    vk::BorderColor border_color{vk::BorderColor::eIntOpaqueBlack};
    bool unnormalized_coordinates{false};
    vk::SamplerCreateFlags flags{};
};

class VulkanSampler {
public:
    VulkanSampler() = default;
    VulkanSampler(const VulkanDevice& device, const VulkanSamplerCreateInfo& info = {});

    VulkanSampler(const VulkanSampler&) = delete;
    VulkanSampler& operator=(const VulkanSampler&) = delete;
    VulkanSampler(VulkanSampler&&) noexcept = default;
    VulkanSampler& operator=(VulkanSampler&&) noexcept = default;

    const vk::raii::Sampler& handle() const noexcept;

private:
    vk::raii::Sampler m_sampler{nullptr};
};

} // namespace arti::renderer::vulkan
