#include "passes/uniform_buffer/uniform_buffer_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_binding_set.h"
#include "artichoco/renderer/vulkan/vulkan_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_command_recorder.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"

#include <array>
#include <cmath>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace arti::renderer_showcase {
namespace {

struct DemoUniforms {
    std::array<float, 4> transform;
    std::array<float, 4> tint;
};

static_assert(std::is_standard_layout_v<DemoUniforms>);
static_assert(sizeof(DemoUniforms) == sizeof(float) * 8);

} // namespace

struct UniformBufferPass::Impl {
    std::vector<renderer::vulkan::VulkanBuffer> uniform_buffers;
    std::vector<renderer::vulkan::VulkanBindingSet> binding_sets;
};

UniformBufferPass::UniformBufferPass(renderer::VertexBuffer vertex_buffer,
                                     renderer::IndexBuffer index_buffer,
                                     std::filesystem::path shader_path)
    : IndexedGraphicsPass(std::move(vertex_buffer),
                          std::move(index_buffer),
                          std::move(shader_path),
                          {0.04f, 0.035f, 0.015f, 1.0f}),
      m_impl(std::make_unique<Impl>())
{}

UniformBufferPass::~UniformBufferPass() = default;

void UniformBufferPass::prepareResources(renderer::vulkan::VulkanPassPrepareContext& context)
{
    if (!m_impl->uniform_buffers.empty()) {
        return;
    }

    renderer::vulkan::VulkanBufferCreateInfo buffer_info;
    buffer_info.size = sizeof(DemoUniforms);
    buffer_info.usage = vk::BufferUsageFlagBits::eUniformBuffer;
    buffer_info.memory = renderer::vulkan::VulkanBufferMemory::HostVisible;

    m_impl->uniform_buffers.reserve(context.frameSlotCount());
    m_impl->binding_sets.reserve(context.frameSlotCount());
    for (size_t index = 0; index < context.frameSlotCount(); ++index) {
        m_impl->uniform_buffers.emplace_back(context.allocator(), buffer_info);
        m_impl->binding_sets.emplace_back(context.device(), context.descriptorAllocator(), bindingLayout());
        m_impl->binding_sets.back().writeUniformBuffer("demo_uniforms", m_impl->uniform_buffers.back());
    }
}

void UniformBufferPass::bindResources(renderer::vulkan::VulkanPassContext& context,
                                      const renderer::vulkan::VulkanPipeline& pipeline)
{
    const float time = elapsedTime();
    DemoUniforms uniforms;
    uniforms.transform = {
        0.12f * std::cos(time * 0.7f),
        0.14f * std::sin(time * 0.9f),
        0.76f + 0.16f * std::cos(time * 1.2f),
        time,
    };
    uniforms.tint = {
        0.75f + 0.25f * std::cos(time),
        0.78f + 0.22f * std::cos(time + 2.0f),
        0.8f + 0.2f * std::cos(time + 4.0f),
        1.0f,
    };

    const size_t slot = context.frame().frameSlotIndex();
    m_impl->uniform_buffers.at(slot).write(std::as_bytes(std::span<const DemoUniforms>{&uniforms, 1}));
    context.commands().bindBindingSet(pipeline, m_impl->binding_sets.at(slot));
}

} // namespace arti::renderer_showcase
