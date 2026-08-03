#pragma once
#include "vulkan_device.h"

#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

class VulkanTextureDescriptors {
public:
    explicit VulkanTextureDescriptors(const VulkanDevice& device);

    VulkanTextureDescriptors(const VulkanTextureDescriptors&) = delete;
    VulkanTextureDescriptors& operator=(const VulkanTextureDescriptors&) = delete;

    vk::raii::DescriptorSet createTextureSet(
        const vk::raii::ImageView& image_view,
        const vk::raii::Sampler& sampler) const;
    const vk::raii::DescriptorSetLayout& layout() const noexcept;
    const vk::raii::Device& device() const noexcept;

private:
    const VulkanDevice& m_device;
    vk::raii::DescriptorSetLayout m_layout{nullptr};
    vk::raii::DescriptorPool m_pool{nullptr};
};

} // namespace arti::renderer::vulkan
