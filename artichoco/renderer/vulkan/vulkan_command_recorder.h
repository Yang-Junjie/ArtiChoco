#pragma once
#include "vulkan_device.h"

#include <cstddef>

#include <span>
#include <type_traits>
#include <vulkan/vulkan_raii.hpp>

namespace arti::renderer::vulkan {

class VulkanBindingSet;
class VulkanComputePipeline;
class VulkanPipeline;

class VulkanCommandRecorder {
public:
    VulkanCommandRecorder(const VulkanDevice& device, const vk::raii::CommandBuffer& command_buffer) noexcept;

    void begin(vk::CommandBufferUsageFlags usage = vk::CommandBufferUsageFlagBits::eOneTimeSubmit) const;
    void end() const;

    void pipelineBarrier(std::span<const vk::MemoryBarrier2> memory_barriers = {},
                         std::span<const vk::BufferMemoryBarrier2> buffer_barriers = {},
                         std::span<const vk::ImageMemoryBarrier2> image_barriers = {},
                         vk::DependencyFlags dependency_flags = {}) const;
    void bufferBarrier(const vk::BufferMemoryBarrier2& barrier) const;
    void imageBarrier(const vk::ImageMemoryBarrier2& barrier) const;

    void beginRendering(const vk::RenderingInfo& rendering_info) const;
    void endRendering() const;
    void setViewportAndScissor(vk::Extent2D extent) const;

    void bindPipeline(const VulkanPipeline& pipeline) const;
    void bindPipeline(const VulkanComputePipeline& pipeline) const;
    void bindBindingSet(const VulkanPipeline& pipeline, const VulkanBindingSet& bindings) const;
    void bindBindingSet(const VulkanComputePipeline& pipeline, const VulkanBindingSet& bindings) const;
    void bindVertexBuffer(vk::Buffer buffer, vk::DeviceSize offset = 0) const;
    void bindIndexBuffer(vk::Buffer buffer, vk::IndexType type, vk::DeviceSize offset = 0) const;

    template <typename T>
    void pushConstants(vk::PipelineLayout layout, vk::ShaderStageFlags stages, uint32_t offset, const T& value) const
    {
        static_assert(std::is_trivially_copyable_v<T>);
        const vk::ArrayProxy<const std::byte> bytes{
            sizeof(T),
            reinterpret_cast<const std::byte*>(&value),
        };
        m_command_buffer.pushConstants<std::byte>(layout, stages, offset, bytes);
    }

    void dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z = 1) const;
    void drawIndexed(uint32_t index_count,
                     uint32_t instance_count = 1,
                     uint32_t first_index = 0,
                     int32_t vertex_offset = 0,
                     uint32_t first_instance = 0) const;

    const vk::raii::CommandBuffer& handle() const noexcept;

private:
    const VulkanDevice& m_device;
    const vk::raii::CommandBuffer& m_command_buffer;
};

} // namespace arti::renderer::vulkan
