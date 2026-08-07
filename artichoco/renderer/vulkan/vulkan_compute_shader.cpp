#include "vulkan_compute_shader.h"

#include <stdexcept>

namespace arti::renderer::vulkan {

VulkanComputeShader::VulkanComputeShader(const VulkanDevice& device, CompiledComputeProgram program)
    : m_entry_point(std::move(program.compute.entry_point)),
      m_group_size_x(program.thread_group_size_x),
      m_group_size_y(program.thread_group_size_y),
      m_group_size_z(program.thread_group_size_z)
{
    if (program.compute.spirv.empty() || m_entry_point.empty() || m_group_size_x == 0 || m_group_size_y == 0 ||
        m_group_size_z == 0) {
        throw std::invalid_argument("A compute shader requires SPIR-V, an entry point, and a non-zero group size.");
    }
    vk::ShaderModuleCreateInfo create_info{};
    create_info.setCode(program.compute.spirv);
    m_module = vk::raii::ShaderModule{device.device(), create_info};
}

vk::PipelineShaderStageCreateInfo VulkanComputeShader::stage() const noexcept
{
    vk::PipelineShaderStageCreateInfo stage_info{};
    stage_info.setStage(vk::ShaderStageFlagBits::eCompute).setModule(*m_module).setPName(m_entry_point.c_str());
    return stage_info;
}

uint32_t VulkanComputeShader::groupSizeX() const noexcept
{
    return m_group_size_x;
}

uint32_t VulkanComputeShader::groupSizeY() const noexcept
{
    return m_group_size_y;
}

uint32_t VulkanComputeShader::groupSizeZ() const noexcept
{
    return m_group_size_z;
}

} // namespace arti::renderer::vulkan
