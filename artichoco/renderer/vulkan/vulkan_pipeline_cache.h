#pragma once
#include "vulkan_compute_pipeline.h"
#include "vulkan_device.h"
#include "vulkan_pipeline.h"

#include <cstddef>

#include <memory>
#include <unordered_map>

namespace arti::renderer::vulkan {

class VulkanPipelineCache {
public:
    explicit VulkanPipelineCache(const VulkanDevice& device);

    VulkanPipelineCache(const VulkanPipelineCache&) = delete;
    VulkanPipelineCache& operator=(const VulkanPipelineCache&) = delete;

    const VulkanPipeline& getGraphics(const VulkanShader& shader,
                                      const VulkanBindingLayout& binding_layout,
                                      const VertexBufferLayout& vertex_layout,
                                      const VulkanGraphicsPipelineCreateInfo& info);

    const VulkanComputePipeline& getCompute(const VulkanComputeShader& shader,
                                            const VulkanBindingLayout& binding_layout);

    void clear() noexcept;

private:
    struct GraphicsPipelineKey {
        const VulkanShader* shader{nullptr};
        const VulkanBindingLayout* binding_layout{nullptr};
        VertexBufferLayout vertex_layout;
        VulkanGraphicsPipelineCreateInfo info;

        bool operator==(const GraphicsPipelineKey&) const noexcept;
    };

    struct GraphicsPipelineKeyHash {
        size_t operator()(const GraphicsPipelineKey&) const noexcept;
    };

    struct ComputePipelineKey {
        const VulkanComputeShader* shader{nullptr};
        const VulkanBindingLayout* binding_layout{nullptr};

        bool operator==(const ComputePipelineKey&) const noexcept = default;
    };

    struct ComputePipelineKeyHash {
        size_t operator()(const ComputePipelineKey&) const noexcept;
    };

    const VulkanDevice& m_device;
    std::unordered_map<GraphicsPipelineKey, std::unique_ptr<VulkanPipeline>, GraphicsPipelineKeyHash> m_graphics;
    std::unordered_map<ComputePipelineKey, std::unique_ptr<VulkanComputePipeline>, ComputePipelineKeyHash> m_compute;
};

} // namespace arti::renderer::vulkan
