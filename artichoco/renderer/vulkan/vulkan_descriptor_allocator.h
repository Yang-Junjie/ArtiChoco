#pragma once
#include "vulkan_device.h"

#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

class VulkanDescriptorAllocator {
public:
    explicit VulkanDescriptorAllocator(const VulkanDevice& device);

    VulkanDescriptorAllocator(const VulkanDescriptorAllocator&) = delete;
    VulkanDescriptorAllocator& operator=(const VulkanDescriptorAllocator&) = delete;

    vk::raii::DescriptorSet allocate(vk::DescriptorSetLayout layout);

private:
    vk::raii::DescriptorPool createPool(uint32_t maximum_sets) const;

    const VulkanDevice& m_device;
    std::vector<vk::raii::DescriptorPool> m_pools;
    uint32_t m_next_pool_size{64};
};

} // namespace arti::renderer::vulkan
