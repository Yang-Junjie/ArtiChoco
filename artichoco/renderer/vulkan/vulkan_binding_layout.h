#pragma once
#include "artichoco/renderer/slang_compiler.h"
#include "vulkan_device.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

struct VulkanReflectedBinding {
    std::string name;
    vk::DescriptorType type{vk::DescriptorType::eSampledImage};
    uint32_t set{0};
    uint32_t binding{0};
    uint32_t count{1};
    vk::ShaderStageFlags stages{};
};

class VulkanBindingLayout {
public:
    VulkanBindingLayout(const VulkanDevice& device, const ShaderReflection& reflection);

    VulkanBindingLayout(const VulkanBindingLayout&) = delete;
    VulkanBindingLayout& operator=(const VulkanBindingLayout&) = delete;

    const VulkanReflectedBinding& find(std::string_view name) const;
    std::span<const vk::DescriptorSetLayout> setLayouts() const noexcept;
    std::span<const vk::PushConstantRange> pushConstantRanges() const noexcept;
    size_t setCount() const noexcept;

private:
    std::vector<VulkanReflectedBinding> m_bindings;
    std::vector<vk::raii::DescriptorSetLayout> m_set_layouts;
    std::vector<vk::DescriptorSetLayout> m_set_layout_handles;
    std::vector<vk::PushConstantRange> m_push_constant_ranges;
};

} // namespace arti::renderer::vulkan
