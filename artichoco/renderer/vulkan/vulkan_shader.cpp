#include "vulkan_shader.h"

namespace arti::renderer::vulkan {

VulkanShader::VulkanShader(
    const VulkanDevice& device,
    std::span<const uint32_t> vertex_spirv,
    std::string vertex_entry_point,
    std::span<const uint32_t> fragment_spirv,
    std::string fragment_entry_point)
    : m_vertex_entry_point(std::move(vertex_entry_point)),
      m_fragment_entry_point(std::move(fragment_entry_point))
{
    vk::ShaderModuleCreateInfo vertex_info{};
    vertex_info.setCode(vertex_spirv);
    m_vertex_module = vk::raii::ShaderModule{device.device(), vertex_info};

    vk::ShaderModuleCreateInfo fragment_info{};
    fragment_info.setCode(fragment_spirv);
    m_fragment_module = vk::raii::ShaderModule{device.device(), fragment_info};
}

std::array<vk::PipelineShaderStageCreateInfo, 2> VulkanShader::stages() const noexcept
{
    vk::PipelineShaderStageCreateInfo vertex_stage{};
    vertex_stage.setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(*m_vertex_module)
        .setPName(m_vertex_entry_point.c_str());

    vk::PipelineShaderStageCreateInfo fragment_stage{};
    fragment_stage.setStage(vk::ShaderStageFlagBits::eFragment)
        .setModule(*m_fragment_module)
        .setPName(m_fragment_entry_point.c_str());
    return {vertex_stage, fragment_stage};
}

} // namespace arti::renderer::vulkan
