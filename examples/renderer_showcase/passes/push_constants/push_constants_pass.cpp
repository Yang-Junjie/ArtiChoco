#include "passes/push_constants/push_constants_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_binding_layout.h"
#include "artichoco/renderer/vulkan/vulkan_command_recorder.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "artichoco/renderer/vulkan/vulkan_pipeline.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace arti::renderer_showcase {
namespace {

struct PushConstants {
    std::array<float, 4> transform;
    std::array<float, 4> tint;
};

static_assert(std::is_standard_layout_v<PushConstants>);
static_assert(sizeof(PushConstants) == sizeof(float) * 8);

} // namespace

PushConstantsPass::PushConstantsPass(renderer::VertexBuffer vertex_buffer,
                                     renderer::IndexBuffer index_buffer,
                                     std::filesystem::path shader_path)
    : IndexedGraphicsPass(std::move(vertex_buffer),
                          std::move(index_buffer),
                          std::move(shader_path),
                          {0.045f, 0.02f, 0.055f, 1.0f})
{}

PushConstantsPass::~PushConstantsPass() = default;

void PushConstantsPass::prepareResources(renderer::vulkan::VulkanPassPrepareContext&)
{
    if (bindingLayout().pushConstantRanges().empty()) {
        throw std::invalid_argument("PushConstantsPass requires a reflected push-constant range.");
    }
}

void PushConstantsPass::bindResources(renderer::vulkan::VulkanPassContext& context,
                                      const renderer::vulkan::VulkanPipeline& pipeline)
{
    const float time = elapsedTime();
    PushConstants constants;
    constants.transform = {
        0.16f * std::sin(time * 0.8f),
        0.1f * std::cos(time * 0.65f),
        0.82f + 0.12f * std::sin(time),
        time,
    };
    constants.tint = {
        0.7f + 0.3f * std::sin(time * 1.1f),
        0.75f + 0.25f * std::sin(time * 1.3f + 2.0f),
        0.8f + 0.2f * std::sin(time * 1.7f + 4.0f),
        1.0f,
    };

    const vk::PushConstantRange& range = bindingLayout().pushConstantRanges().front();
    context.commands().pushConstants(*pipeline.layout(), range.stageFlags, range.offset, constants);
}

} // namespace arti::renderer_showcase
