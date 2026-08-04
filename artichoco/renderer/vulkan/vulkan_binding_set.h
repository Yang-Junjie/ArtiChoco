#pragma once
#include "vulkan_binding_layout.h"
#include "vulkan_descriptor_allocator.h"

#include <span>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

class VulkanBindingSet {
public:
    VulkanBindingSet(const VulkanDevice& device,
                     VulkanDescriptorAllocator& allocator,
                     const VulkanBindingLayout& layout);

    VulkanBindingSet(const VulkanBindingSet&) = delete;
    VulkanBindingSet& operator=(const VulkanBindingSet&) = delete;
    VulkanBindingSet(VulkanBindingSet&&) noexcept = default;
    VulkanBindingSet& operator=(VulkanBindingSet&&) noexcept = delete;

    void writeSampledImage(std::string_view name,
                           vk::ImageView image_view,
                           vk::ImageLayout image_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
                           uint32_t array_element = 0);
    void writeStorageImage(std::string_view name,
                           vk::ImageView image_view,
                           vk::ImageLayout image_layout = vk::ImageLayout::eGeneral,
                           uint32_t array_element = 0);
    void writeSampler(std::string_view name, vk::Sampler sampler, uint32_t array_element = 0);
    void writeCombinedImageSampler(std::string_view name,
                                   vk::ImageView image_view,
                                   vk::Sampler sampler,
                                   vk::ImageLayout image_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
                                   uint32_t array_element = 0);
    void writeBuffer(std::string_view name,
                     vk::Buffer buffer,
                     vk::DeviceSize offset,
                     vk::DeviceSize range,
                     uint32_t array_element = 0);

    std::span<const vk::DescriptorSet> descriptorSets() const noexcept;

private:
    const VulkanReflectedBinding&
        requireBinding(std::string_view name, vk::DescriptorType expected, uint32_t array_element) const;

    const VulkanDevice& m_device;
    const VulkanBindingLayout& m_layout;
    std::vector<vk::raii::DescriptorSet> m_sets;
    std::vector<vk::DescriptorSet> m_set_handles;
};

} // namespace arti::renderer::vulkan
