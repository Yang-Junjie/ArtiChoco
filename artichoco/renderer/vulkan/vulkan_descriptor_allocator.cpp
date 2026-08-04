#include "vulkan_descriptor_allocator.h"

#include <array>
#include <utility>

namespace arti::renderer::vulkan {

VulkanDescriptorAllocator::VulkanDescriptorAllocator(const VulkanDevice& device)
    : m_device(device)
{
    m_pools.push_back(createPool(m_next_pool_size));
    m_next_pool_size *= 2;
}

vk::raii::DescriptorSet VulkanDescriptorAllocator::allocate(vk::DescriptorSetLayout layout)
{
    const auto allocate_from = [this, layout](const vk::raii::DescriptorPool& pool) {
        const std::array layouts = {layout};
        vk::DescriptorSetAllocateInfo allocate_info{};
        allocate_info.setDescriptorPool(*pool).setSetLayouts(layouts);
        vk::raii::DescriptorSets sets{m_device.device(), allocate_info};
        return std::move(sets.front());
    };

    try {
        return allocate_from(m_pools.back());
    } catch (const vk::OutOfPoolMemoryError&) {
    } catch (const vk::FragmentedPoolError&) {
    }

    m_pools.push_back(createPool(m_next_pool_size));
    m_next_pool_size *= 2;
    return allocate_from(m_pools.back());
}

vk::raii::DescriptorPool VulkanDescriptorAllocator::createPool(uint32_t maximum_sets) const
{
    const uint32_t descriptors_per_type = maximum_sets * 8;
    const std::array pool_sizes = {
        vk::DescriptorPoolSize{vk::DescriptorType::eSampler, descriptors_per_type},
        vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, descriptors_per_type},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, descriptors_per_type},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, descriptors_per_type},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, descriptors_per_type},
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformTexelBuffer, descriptors_per_type},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageTexelBuffer, descriptors_per_type},
        vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, descriptors_per_type},
    };
    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(maximum_sets)
        .setPoolSizes(pool_sizes);
    return vk::raii::DescriptorPool{m_device.device(), pool_info};
}

} // namespace arti::renderer::vulkan
