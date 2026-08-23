#include "texture_compute_pass.h"

#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/texture_2d.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"

#include <array>
#include <stdexcept>
#include <utility>

namespace arti::test_app {

struct TextureComputePass::Impl {
    Impl(std::shared_ptr<renderer::Texture2D> source, std::filesystem::path shader_path)
        : source(std::move(source)), shader_path(std::move(shader_path))
    {}

    std::shared_ptr<renderer::Texture2D> source;
    std::filesystem::path shader_path;
    renderer::ShaderReflection reflection;
    nvrhi::ShaderHandle shader;
    nvrhi::BindingLayoutHandle binding_layout;
    nvrhi::ComputePipelineHandle pipeline;
    nvrhi::SamplerHandle sampler;
    nvrhi::TextureHandle output;
    uint32_t group_size_x{1};
    uint32_t group_size_y{1};
    float time{0.0f};
};

TextureComputePass::TextureComputePass(std::shared_ptr<renderer::Texture2D> source,
        const std::filesystem::path& shader_path)
    : m_impl(std::make_unique<Impl>(std::move(source), shader_path))
{}

TextureComputePass::~TextureComputePass() = default;

void TextureComputePass::applyFrameData(const RenderFrameData& frame_data)
{
    m_impl->source = frame_data.draws.empty() ? nullptr : frame_data.draws.front().base_color_texture;
    m_impl->time = frame_data.time;
}

void TextureComputePass::prepare(renderer::RenderPassPrepareContext& context)
{
    if (!m_impl->source) {
        throw std::logic_error("TextureComputePass requires a source texture.");
    }
    if (!m_impl->pipeline) {
        const renderer::CompiledComputeProgram program =
                renderer::SlangCompiler::compileCompute({m_impl->shader_path});
        const auto shaders = renderer::vulkan::createNvrhiComputeShaderSet(
                context.device(), program, "ArtiChoco NVRHI texture compute");
        if (shaders.binding_layouts.empty() || !shaders.binding_layouts.front()) {
            throw std::runtime_error("TextureComputePass shader has no NVRHI binding layout.");
        }

        m_impl->shader = shaders.compute_shader;
        m_impl->binding_layout = shaders.binding_layouts.front();
        m_impl->reflection = program.reflection;
        m_impl->group_size_x = program.thread_group_size_x;
        m_impl->group_size_y = program.thread_group_size_y;

        nvrhi::ComputePipelineDesc pipeline_desc;
        pipeline_desc.setComputeShader(m_impl->shader);
        for (const nvrhi::BindingLayoutHandle& layout : shaders.binding_layouts) {
            if (layout) {
                pipeline_desc.addBindingLayout(layout);
            }
        }
        m_impl->pipeline = context.device().createComputePipeline(pipeline_desc);
        m_impl->sampler = context.device().createSampler(nvrhi::SamplerDesc{});
        if (!m_impl->pipeline || !m_impl->sampler) {
            throw std::runtime_error("NVRHI failed to create TextureComputePass resources.");
        }
    }

    const bool size_changed = !m_impl->output ||
            m_impl->output->getDesc().width != m_impl->source->width() ||
            m_impl->output->getDesc().height != m_impl->source->height();
    if (size_changed) {
        nvrhi::TextureDesc output_desc;
        output_desc.setWidth(m_impl->source->width())
                .setHeight(m_impl->source->height())
                .setFormat(nvrhi::Format::RGBA8_UNORM)
                .setIsUAV(true)
                .setDebugName("ArtiChoco NVRHI texture compute output")
                .enableAutomaticStateTracking(nvrhi::ResourceStates::UnorderedAccess);
        m_impl->output = context.device().createTexture(output_desc);
        if (!m_impl->output) {
            throw std::runtime_error("NVRHI failed to create TextureComputePass output.");
        }
    }
}

void TextureComputePass::record(renderer::RenderPassContext& context)
{
    if (!m_impl->source || !m_impl->output || !m_impl->pipeline) {
        return;
    }

    const std::array resources = {
            renderer::vulkan::NvrhiBindingResource::Texture(
                    "source_texture", context.texture(*m_impl->source)),
            renderer::vulkan::NvrhiBindingResource::Sampler(
                    "source_sampler", *m_impl->sampler),
            renderer::vulkan::NvrhiBindingResource::Texture(
                    "output_texture", *m_impl->output),
    };
    const nvrhi::BindingSetHandle binding_set = renderer::vulkan::createNvrhiBindingSet(
            context.device(), m_impl->reflection, 0, *m_impl->binding_layout, resources);

    nvrhi::ComputeState state;
    state.setPipeline(m_impl->pipeline).addBindingSet(binding_set);
    auto& commands = context.commands();
    commands.setComputeState(state);
    commands.setPushConstants(&m_impl->time, sizeof(m_impl->time));
    commands.dispatch(
            (m_impl->source->width() + m_impl->group_size_x - 1) / m_impl->group_size_x,
            (m_impl->source->height() + m_impl->group_size_y - 1) / m_impl->group_size_y,
            1);
}

nvrhi::ITexture& TextureComputePass::output() const
{
    if (!m_impl->output) {
        throw std::logic_error("TextureComputePass output is not initialized.");
    }
    return *m_impl->output;
}

} // namespace arti::test_app
