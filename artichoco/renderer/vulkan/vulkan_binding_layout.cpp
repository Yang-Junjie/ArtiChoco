#include "vulkan_binding_layout.h"

#include <algorithm>
#include <stdexcept>

namespace arti::renderer::vulkan {
namespace {

vk::DescriptorType toVulkanDescriptorType(ShaderResourceType type)
{
    switch (type) {
        case ShaderResourceType::Sampler:
            return vk::DescriptorType::eSampler;
        case ShaderResourceType::SampledImage:
            return vk::DescriptorType::eSampledImage;
        case ShaderResourceType::StorageImage:
            return vk::DescriptorType::eStorageImage;
        case ShaderResourceType::UniformBuffer:
            return vk::DescriptorType::eUniformBuffer;
        case ShaderResourceType::StorageBuffer:
            return vk::DescriptorType::eStorageBuffer;
        case ShaderResourceType::UniformTexelBuffer:
            return vk::DescriptorType::eUniformTexelBuffer;
        case ShaderResourceType::StorageTexelBuffer:
            return vk::DescriptorType::eStorageTexelBuffer;
        case ShaderResourceType::CombinedImageSampler:
            return vk::DescriptorType::eCombinedImageSampler;
    }
    throw std::invalid_argument("Unsupported reflected shader resource type.");
}

vk::ShaderStageFlags toVulkanShaderStages(ShaderStageMask stages)
{
    vk::ShaderStageFlags result;
    const auto mask = static_cast<uint32_t>(stages);
    if (mask & static_cast<uint32_t>(ShaderStageMask::Vertex)) {
        result |= vk::ShaderStageFlagBits::eVertex;
    }
    if (mask & static_cast<uint32_t>(ShaderStageMask::Fragment)) {
        result |= vk::ShaderStageFlagBits::eFragment;
    }
    if (mask & static_cast<uint32_t>(ShaderStageMask::Compute)) {
        result |= vk::ShaderStageFlagBits::eCompute;
    }
    return result;
}

} // namespace

VulkanBindingLayout::VulkanBindingLayout(const VulkanDevice& device, const ShaderReflection& reflection)
{
    uint32_t maximum_set = 0;
    bool has_bindings = false;
    m_bindings.reserve(reflection.bindings.size());
    for (const auto& reflected : reflection.bindings) {
        if (reflected.name.empty() || reflected.count == 0) {
            throw std::invalid_argument("Reflected descriptor bindings require a name and non-zero count.");
        }
        const auto duplicate = std::ranges::find_if(m_bindings, [&reflected](const VulkanReflectedBinding& binding) {
            return binding.name == reflected.name ||
                   (binding.set == reflected.set && binding.binding == reflected.binding);
        });
        if (duplicate != m_bindings.end()) {
            throw std::invalid_argument("Reflected descriptor bindings must have unique names and locations.");
        }
        m_bindings.push_back({
            reflected.name,
            toVulkanDescriptorType(reflected.type),
            reflected.set,
            reflected.binding,
            reflected.count,
            toVulkanShaderStages(reflected.stages),
        });
        maximum_set = std::max(maximum_set, reflected.set);
        has_bindings = true;
    }

    if (has_bindings) {
        m_set_layouts.reserve(static_cast<size_t>(maximum_set) + 1);
        m_set_layout_handles.reserve(static_cast<size_t>(maximum_set) + 1);
        for (uint32_t set = 0; set <= maximum_set; ++set) {
            std::vector<vk::DescriptorSetLayoutBinding> bindings;
            for (const auto& reflected : m_bindings) {
                if (reflected.set == set) {
                    bindings.push_back({
                        reflected.binding,
                        reflected.type,
                        reflected.count,
                        reflected.stages,
                        nullptr,
                    });
                }
            }
            std::ranges::sort(bindings, {}, &vk::DescriptorSetLayoutBinding::binding);
            vk::DescriptorSetLayoutCreateInfo layout_info{};
            layout_info.setBindings(bindings);
            m_set_layouts.emplace_back(device.device(), layout_info);
            m_set_layout_handles.push_back(*m_set_layouts.back());
        }
    }

    m_push_constant_ranges.reserve(reflection.push_constants.size());
    for (const auto& reflected : reflection.push_constants) {
        if (reflected.size == 0) {
            throw std::invalid_argument("Reflected push-constant ranges require a non-zero size.");
        }
        m_push_constant_ranges.push_back({
            toVulkanShaderStages(reflected.stages),
            reflected.offset,
            reflected.size,
        });
    }
}

const VulkanReflectedBinding& VulkanBindingLayout::find(std::string_view name) const
{
    const auto binding = std::ranges::find_if(m_bindings, [name](const VulkanReflectedBinding& candidate) {
        return candidate.name == name;
    });
    if (binding == m_bindings.end()) {
        throw std::out_of_range("Shader resource binding was not found: " + std::string{name});
    }
    return *binding;
}

std::span<const vk::DescriptorSetLayout> VulkanBindingLayout::setLayouts() const noexcept
{
    return m_set_layout_handles;
}

std::span<const vk::PushConstantRange> VulkanBindingLayout::pushConstantRanges() const noexcept
{
    return m_push_constant_ranges;
}

size_t VulkanBindingLayout::setCount() const noexcept
{
    return m_set_layouts.size();
}

} // namespace arti::renderer::vulkan
