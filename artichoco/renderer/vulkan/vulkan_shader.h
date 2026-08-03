#pragma once
#include "vulkan_device.h"

#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace arti::renderer::vulkan {

class VulkanShader {
public:
    VulkanShader(
        const VulkanDevice& device,
        std::span<const uint32_t> vertex_spirv,
        std::string vertex_entry_point,
        std::span<const uint32_t> fragment_spirv,
        std::string fragment_entry_point);

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
