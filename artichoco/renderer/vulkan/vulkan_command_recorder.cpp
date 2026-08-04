#include "vulkan_binding_set.h"
#include "vulkan_command_recorder.h"
#include "vulkan_compute_pipeline.h"
#include "vulkan_pipeline.h"

#include <array>

namespace arti::renderer::vulkan {

VulkanCommandRecorder::VulkanCommandRecorder(const VulkanDevice& device,
                                             const vk::raii::CommandBuffer& command_buffer) noexcept
    : m_device(device),
      m_command_buffer(command_buffer)
{}

void VulkanCommandRecorder::begin(vk::CommandBufferUsageFlags usage) const
{
    vk::CommandBufferBeginInfo begin_info{};
    begin_info.setFlags(usage);
    m_command_buffer.begin(begin_info);
}

void VulkanCommandRecorder::end() const
{
    m_command_buffer.end();
}

void VulkanCommandRecorder::pipelineBarrier(std::span<const vk::MemoryBarrier2> memory_barriers,
                                            std::span<const vk::BufferMemoryBarrier2> buffer_barriers,
                                            std::span<const vk::ImageMemoryBarrier2> image_barriers,
                                            vk::DependencyFlags dependency_flags) const
{
    vk::DependencyInfo dependency_info{};
    dependency_info.setDependencyFlags(dependency_flags)
        .setMemoryBarrierCount(static_cast<uint32_t>(memory_barriers.size()))
        .setPMemoryBarriers(memory_barriers.data())
        .setBufferMemoryBarrierCount(static_cast<uint32_t>(buffer_barriers.size()))
        .setPBufferMemoryBarriers(buffer_barriers.data())
        .setImageMemoryBarrierCount(static_cast<uint32_t>(image_barriers.size()))
        .setPImageMemoryBarriers(image_barriers.data());
    if (m_device.usesCore13()) {
        m_command_buffer.pipelineBarrier2(dependency_info);
    } else {
        m_command_buffer.pipelineBarrier2KHR(dependency_info);
    }
}

void VulkanCommandRecorder::bufferBarrier(const vk::BufferMemoryBarrier2& barrier) const
{
    pipelineBarrier({}, std::span{&barrier, 1}, {});
}

void VulkanCommandRecorder::imageBarrier(const vk::ImageMemoryBarrier2& barrier) const
{
    pipelineBarrier({}, {}, std::span{&barrier, 1});
}

void VulkanCommandRecorder::beginRendering(const vk::RenderingInfo& rendering_info) const
{
    if (m_device.usesCore13()) {
        m_command_buffer.beginRendering(rendering_info);
    } else {
        m_command_buffer.beginRenderingKHR(rendering_info);
    }
}

void VulkanCommandRecorder::endRendering() const
{
    if (m_device.usesCore13()) {
        m_command_buffer.endRendering();
    } else {
        m_command_buffer.endRenderingKHR();
    }
}

void VulkanCommandRecorder::setViewportAndScissor(vk::Extent2D extent) const
{
    const std::array viewports = {
        vk::Viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f},
    };
    const std::array scissors = {vk::Rect2D{{0, 0}, extent}};
    m_command_buffer.setViewport(0, viewports);
    m_command_buffer.setScissor(0, scissors);
}

void VulkanCommandRecorder::bindPipeline(const VulkanPipeline& pipeline) const
{
    m_command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.handle());
}

void VulkanCommandRecorder::bindPipeline(const VulkanComputePipeline& pipeline) const
{
    m_command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline.handle());
}

void VulkanCommandRecorder::bindBindingSet(const VulkanPipeline& pipeline, const VulkanBindingSet& bindings) const
{
    m_command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, *pipeline.layout(), 0, bindings.descriptorSets(), {});
}

void VulkanCommandRecorder::bindBindingSet(const VulkanComputePipeline& pipeline,
                                           const VulkanBindingSet& bindings) const
{
    m_command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, *pipeline.layout(), 0, bindings.descriptorSets(), {});
}

void VulkanCommandRecorder::bindVertexBuffer(vk::Buffer buffer, vk::DeviceSize offset) const
{
    const std::array buffers = {buffer};
    const std::array offsets = {offset};
    m_command_buffer.bindVertexBuffers(0, buffers, offsets);
}

void VulkanCommandRecorder::bindIndexBuffer(vk::Buffer buffer, vk::IndexType type, vk::DeviceSize offset) const
{
    m_command_buffer.bindIndexBuffer(buffer, offset, type);
}

void VulkanCommandRecorder::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) const
{
    m_command_buffer.dispatch(group_count_x, group_count_y, group_count_z);
}

void VulkanCommandRecorder::drawIndexed(uint32_t index_count,
                                        uint32_t instance_count,
                                        uint32_t first_index,
                                        int32_t vertex_offset,
                                        uint32_t first_instance) const
{
    m_command_buffer.drawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
}

const vk::raii::CommandBuffer& VulkanCommandRecorder::handle() const noexcept
{
    return m_command_buffer;
}

} // namespace arti::renderer::vulkan
