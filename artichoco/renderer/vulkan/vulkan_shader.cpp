#include "vulkan_shader.h"

namespace arti::renderer::vulkan {

VulkanShader::VulkanShader(const VulkanDevice& device, CompiledGraphicsProgram program)
    : m_vertex_entry_point(std::move(program.vertex.entry_point)),
      m_fragment_entry_point(std::move(program.fragment.entry_point))
{
    vk::ShaderModuleCreateInfo vertex_info{};
    vertex_info.setCode(program.vertex.spirv);
    m_vertex_module = vk::raii::ShaderModule{device.device(), vertex_info};

    vk::ShaderModuleCreateInfo fragment_info{};
    fragment_info.setCode(program.fragment.spirv);
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
