#include "vulkan_pipeline_layout.h"

namespace arti::renderer::vulkan {

vk::raii::PipelineLayout createPipelineLayout(const VulkanDevice& device, const VulkanBindingLayout& binding_layout)
{
    const auto set_layouts = binding_layout.setLayouts();
    const auto push_constant_ranges = binding_layout.pushConstantRanges();
    vk::PipelineLayoutCreateInfo create_info{};
    create_info.setSetLayoutCount(static_cast<uint32_t>(set_layouts.size()))
        .setPSetLayouts(set_layouts.data())
        .setPushConstantRangeCount(static_cast<uint32_t>(push_constant_ranges.size()))
        .setPPushConstantRanges(push_constant_ranges.data());
    return vk::raii::PipelineLayout{device.device(), create_info};
}

} // namespace arti::renderer::vulkan
