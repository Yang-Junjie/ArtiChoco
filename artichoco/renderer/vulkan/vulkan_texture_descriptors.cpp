#include "vulkan_texture_descriptors.h"

#include <array>
#include <utility>

namespace arti::renderer::vulkan {

VulkanTextureDescriptors::VulkanTextureDescriptors(const VulkanDevice& device)
    : m_device(device)
{
    const std::array bindings = {
        vk::DescriptorSetLayoutBinding{
            0, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eFragment, nullptr},
        vk::DescriptorSetLayoutBinding{
            1, vk::DescriptorType::eSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr},
    };
    vk::DescriptorSetLayoutCreateInfo layout_info{};
    layout_info.setBindings(bindings);
    m_layout = vk::raii::DescriptorSetLayout{device.device(), layout_info};

    constexpr uint32_t maximum_textures = 256;
    const std::array pool_sizes = {
        vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, maximum_textures},
        vk::DescriptorPoolSize{vk::DescriptorType::eSampler, maximum_textures},
    };
    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(maximum_textures)
        .setPoolSizes(pool_sizes);
    m_pool = vk::raii::DescriptorPool{device.device(), pool_info};
}

vk::raii::DescriptorSet VulkanTextureDescriptors::createTextureSet(
    const vk::raii::ImageView& image_view,
    const vk::raii::Sampler& sampler) const
{
    const std::array layouts = {*m_layout};
    vk::DescriptorSetAllocateInfo allocate_info{};
    allocate_info.setDescriptorPool(*m_pool).setSetLayouts(layouts);
    vk::raii::DescriptorSets sets{m_device.device(), allocate_info};
    auto descriptor_set = std::move(sets.front());

    vk::DescriptorImageInfo image_info{};
    image_info.setImageView(*image_view).setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::DescriptorImageInfo sampler_info{};
    sampler_info.setSampler(*sampler);
    const std::array writes = {
        vk::WriteDescriptorSet{*descriptor_set, 0, 0, 1, vk::DescriptorType::eSampledImage, &image_info},
        vk::WriteDescriptorSet{*descriptor_set, 1, 0, 1, vk::DescriptorType::eSampler, &sampler_info},
    };
    m_device.device().updateDescriptorSets(writes, {});
    return descriptor_set;
}

const vk::raii::DescriptorSetLayout& VulkanTextureDescriptors::layout() const noexcept
{
    return m_layout;
}

const vk::raii::Device& VulkanTextureDescriptors::device() const noexcept
{
    return m_device.device();
}

} // namespace arti::renderer::vulkan
