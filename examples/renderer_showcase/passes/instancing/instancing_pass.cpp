#include "passes/instancing/instancing_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_binding_set.h"
#include "artichoco/renderer/vulkan/vulkan_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_command_recorder.h"
#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"
#include "artichoco/renderer/vulkan/vulkan_upload_context.h"

#include <array>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace arti::renderer_showcase {
namespace {

struct InstanceData {
    std::array<float, 4> transform;
    std::array<float, 4> color;
};

static_assert(std::is_standard_layout_v<InstanceData>);
static_assert(sizeof(InstanceData) == sizeof(float) * 8);

constexpr std::array instances = {
    InstanceData{{-0.72f, -0.62f, 0.22f, 0.0f}, {1.0f, 0.25f, 0.2f, 1.0f}},
    InstanceData{{-0.24f, -0.62f, 0.22f, 0.0f}, {0.95f, 0.65f, 0.15f, 1.0f}},
    InstanceData{{0.24f, -0.62f, 0.22f, 0.0f}, {0.35f, 0.95f, 0.3f, 1.0f}},
    InstanceData{{0.72f, -0.62f, 0.22f, 0.0f}, {0.2f, 0.75f, 1.0f, 1.0f}},
    InstanceData{{-0.72f, -0.12f, 0.22f, 0.0f}, {0.85f, 0.25f, 0.95f, 1.0f}},
    InstanceData{{-0.24f, -0.12f, 0.22f, 0.0f}, {1.0f, 0.4f, 0.6f, 1.0f}},
    InstanceData{{0.24f, -0.12f, 0.22f, 0.0f}, {0.25f, 1.0f, 0.75f, 1.0f}},
    InstanceData{{0.72f, -0.12f, 0.22f, 0.0f}, {0.45f, 0.5f, 1.0f, 1.0f}},
    InstanceData{{-0.72f, 0.38f, 0.22f, 0.0f}, {0.7f, 0.9f, 0.2f, 1.0f}},
    InstanceData{{-0.24f, 0.38f, 0.22f, 0.0f}, {1.0f, 0.55f, 0.15f, 1.0f}},
    InstanceData{{0.24f, 0.38f, 0.22f, 0.0f}, {0.25f, 0.65f, 1.0f, 1.0f}},
    InstanceData{{0.72f, 0.38f, 0.22f, 0.0f}, {0.95f, 0.3f, 0.65f, 1.0f}},
};

} // namespace

struct InstancingPass::Impl {
    std::unique_ptr<renderer::vulkan::VulkanBuffer> instance_buffer;
    std::vector<renderer::vulkan::VulkanBindingSet> binding_sets;
};

InstancingPass::InstancingPass(renderer::VertexBuffer vertex_buffer,
                               renderer::IndexBuffer index_buffer,
                               std::filesystem::path shader_path)
    : IndexedGraphicsPass(std::move(vertex_buffer),
                          std::move(index_buffer),
                          std::move(shader_path),
                          {0.018f, 0.028f, 0.025f, 1.0f}),
      m_impl(std::make_unique<Impl>())
{}

InstancingPass::~InstancingPass() = default;

void InstancingPass::prepareResources(renderer::vulkan::VulkanPassPrepareContext& context)
{
    if (m_impl->instance_buffer) {
        return;
    }

    renderer::vulkan::VulkanBufferCreateInfo buffer_info;
    buffer_info.size = sizeof(instances);
    buffer_info.usage = vk::BufferUsageFlagBits::eStorageBuffer;
    buffer_info.memory = renderer::vulkan::VulkanBufferMemory::DeviceLocal;
    m_impl->instance_buffer =
        std::make_unique<renderer::vulkan::VulkanBuffer>(context.allocator(), buffer_info);
    const renderer::vulkan::VulkanBufferState vertex_storage_read{
        vk::PipelineStageFlagBits2::eVertexShader,
        vk::AccessFlagBits2::eShaderStorageRead,
    };
    m_impl->instance_buffer->uploadInitial(
        context.uploadContext(), std::as_bytes(std::span{instances}), vertex_storage_read);

    m_impl->binding_sets.reserve(context.frameSlotCount());
    for (size_t index = 0; index < context.frameSlotCount(); ++index) {
        m_impl->binding_sets.emplace_back(context.device(), context.descriptorAllocator(), bindingLayout());
        m_impl->binding_sets.back().writeStorageBuffer("instance_data", *m_impl->instance_buffer);
    }
}

void InstancingPass::bindResources(renderer::vulkan::VulkanPassContext& context,
                                   const renderer::vulkan::VulkanPipeline& pipeline)
{
    context.commands().bindBindingSet(pipeline, m_impl->binding_sets.at(context.frame().frameSlotIndex()));
}

uint32_t InstancingPass::instanceCount() const noexcept
{
    return static_cast<uint32_t>(instances.size());
}

} // namespace arti::renderer_showcase
