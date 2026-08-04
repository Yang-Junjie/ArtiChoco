#include "vulkan_binding_set.h"

#include <stdexcept>

namespace arti::renderer::vulkan {

VulkanBindingSet::VulkanBindingSet(const VulkanDevice& device,
                                   VulkanDescriptorAllocator& allocator,
                                   const VulkanBindingLayout& layout)
    : m_device(device),
      m_layout(layout)
{
    m_sets.reserve(layout.setCount());
    m_set_handles.reserve(layout.setCount());
    for (const vk::DescriptorSetLayout set_layout : layout.setLayouts()) {
        m_sets.push_back(allocator.allocate(set_layout));
        m_set_handles.push_back(*m_sets.back());
    }
}

void VulkanBindingSet::writeSampledImage(std::string_view name,
                                         vk::ImageView image_view,
                                         vk::ImageLayout image_layout,
                                         uint32_t array_element)
{
    const auto& binding = requireBinding(name, vk::DescriptorType::eSampledImage, array_element);
    vk::DescriptorImageInfo image_info{};
    image_info.setImageView(image_view).setImageLayout(image_layout);
    vk::WriteDescriptorSet write{};
    write.setDstSet(m_set_handles.at(binding.set))
        .setDstBinding(binding.binding)
        .setDstArrayElement(array_element)
        .setDescriptorType(binding.type)
        .setImageInfo(image_info);
    m_device.device().updateDescriptorSets(write, {});
}

void VulkanBindingSet::writeStorageImage(std::string_view name,
                                         vk::ImageView image_view,
                                         vk::ImageLayout image_layout,
                                         uint32_t array_element)
{
    const auto& binding = requireBinding(name, vk::DescriptorType::eStorageImage, array_element);
    vk::DescriptorImageInfo image_info{};
    image_info.setImageView(image_view).setImageLayout(image_layout);
    vk::WriteDescriptorSet write{};
    write.setDstSet(m_set_handles.at(binding.set))
        .setDstBinding(binding.binding)
        .setDstArrayElement(array_element)
        .setDescriptorType(binding.type)
        .setImageInfo(image_info);
    m_device.device().updateDescriptorSets(write, {});
}

void VulkanBindingSet::writeSampler(std::string_view name, vk::Sampler sampler, uint32_t array_element)
{
    const auto& binding = requireBinding(name, vk::DescriptorType::eSampler, array_element);
    vk::DescriptorImageInfo image_info{};
    image_info.setSampler(sampler);
    vk::WriteDescriptorSet write{};
    write.setDstSet(m_set_handles.at(binding.set))
        .setDstBinding(binding.binding)
        .setDstArrayElement(array_element)
        .setDescriptorType(binding.type)
        .setImageInfo(image_info);
    m_device.device().updateDescriptorSets(write, {});
}

void VulkanBindingSet::writeCombinedImageSampler(std::string_view name,
                                                 vk::ImageView image_view,
                                                 vk::Sampler sampler,
                                                 vk::ImageLayout image_layout,
                                                 uint32_t array_element)
{
    const auto& binding = requireBinding(name, vk::DescriptorType::eCombinedImageSampler, array_element);
    vk::DescriptorImageInfo image_info{};
    image_info.setImageView(image_view).setSampler(sampler).setImageLayout(image_layout);
    vk::WriteDescriptorSet write{};
    write.setDstSet(m_set_handles.at(binding.set))
        .setDstBinding(binding.binding)
        .setDstArrayElement(array_element)
        .setDescriptorType(binding.type)
        .setImageInfo(image_info);
    m_device.device().updateDescriptorSets(write, {});
}

void VulkanBindingSet::writeBuffer(
    std::string_view name, vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range, uint32_t array_element)
{
    const auto& binding = m_layout.find(name);
    if (binding.type != vk::DescriptorType::eUniformBuffer && binding.type != vk::DescriptorType::eStorageBuffer) {
        throw std::invalid_argument("Shader resource is not a uniform or storage buffer: " + std::string{name});
    }
    if (array_element >= binding.count) {
        throw std::out_of_range("Descriptor array element is out of range: " + std::string{name});
    }
    vk::DescriptorBufferInfo buffer_info{};
    buffer_info.setBuffer(buffer).setOffset(offset).setRange(range);
    vk::WriteDescriptorSet write{};
    write.setDstSet(m_set_handles.at(binding.set))
        .setDstBinding(binding.binding)
        .setDstArrayElement(array_element)
        .setDescriptorType(binding.type)
        .setBufferInfo(buffer_info);
    m_device.device().updateDescriptorSets(write, {});
}

std::span<const vk::DescriptorSet> VulkanBindingSet::descriptorSets() const noexcept
{
    return m_set_handles;
}

const VulkanReflectedBinding&
    VulkanBindingSet::requireBinding(std::string_view name, vk::DescriptorType expected, uint32_t array_element) const
{
    const auto& binding = m_layout.find(name);
    if (binding.type != expected) {
        throw std::invalid_argument("Shader resource has an incompatible descriptor type: " + std::string{name});
    }
    if (array_element >= binding.count) {
        throw std::out_of_range("Descriptor array element is out of range: " + std::string{name});
    }
    return binding;
}

} // namespace arti::renderer::vulkan
