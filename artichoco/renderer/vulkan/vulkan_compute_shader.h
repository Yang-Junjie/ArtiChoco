#pragma once
#include "vulkan_device.h"

#include <cstdint>

#include <span>
#include <string>
#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

class VulkanComputeShader {
public:
    VulkanComputeShader(const VulkanDevice& device,
                        std::span<const uint32_t> spirv,
                        std::string entry_point,
                        uint32_t group_size_x,
                        uint32_t group_size_y,
                        uint32_t group_size_z);

    VulkanComputeShader(const VulkanComputeShader&) = delete;
    VulkanComputeShader& operator=(const VulkanComputeShader&) = delete;

    vk::PipelineShaderStageCreateInfo stage() const noexcept;
    uint32_t groupSizeX() const noexcept;
    uint32_t groupSizeY() const noexcept;
    uint32_t groupSizeZ() const noexcept;

private:
    vk::raii::ShaderModule m_module{nullptr};
    std::string m_entry_point;
    uint32_t m_group_size_x{1};
    uint32_t m_group_size_y{1};
    uint32_t m_group_size_z{1};
};

} // namespace arti::renderer::vulkan
