#pragma once
#include "artichoco/renderer/slang_compiler.h"
#include "vulkan_device.h"

#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <string>

namespace arti::renderer::vulkan {

class VulkanShader {
public:
    VulkanShader(const VulkanDevice& device, CompiledGraphicsProgram program);

    VulkanShader(const VulkanShader&) = delete;
    VulkanShader& operator=(const VulkanShader&) = delete;

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages() const noexcept;

private:
    vk::raii::ShaderModule m_vertex_module{nullptr};
    vk::raii::ShaderModule m_fragment_module{nullptr};
    std::string m_vertex_entry_point;
    std::string m_fragment_entry_point;
};

} // namespace arti::renderer::vulkan
