#include "mrt_composite_pass.h"

#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"

#include <array>
#include <stdexcept>
#include <utility>

namespace arti::test_app {

struct MrtCompositePass::Impl {
    Impl(MrtMeshPass& source, std::filesystem::path shader_path)
        : source(&source), shader_path(std::move(shader_path))
    {}

    MrtMeshPass* source{nullptr};
    std::filesystem::path shader_path;
    renderer::ShaderReflection reflection;
    nvrhi::ShaderHandle vertex_shader;
    nvrhi::ShaderHandle pixel_shader;
    nvrhi::BindingLayoutHandle binding_layout;
    nvrhi::BindingSetHandle binding_set;
    nvrhi::GraphicsPipelineHandle pipeline;
    nvrhi::SamplerHandle sampler;
};

MrtCompositePass::MrtCompositePass(MrtMeshPass& source, const std::filesystem::path& shader_path)
    : m_impl(std::make_unique<Impl>(source, shader_path))
{}

MrtCompositePass::~MrtCompositePass() = default;

void MrtCompositePass::prepare(renderer::RenderPassPrepareContext& context)
{
    if (!m_impl->binding_layout) {
        const renderer::CompiledGraphicsProgram program =
                renderer::SlangCompiler::compileGraphics({m_impl->shader_path});
        const auto shaders = renderer::vulkan::createNvrhiGraphicsShaderSet(
                context.device(), program, "ArtiChoco NVRHI MRT composite");
        if (shaders.binding_layouts.empty() || !shaders.binding_layouts.front()) {
            throw std::runtime_error("MrtCompositePass shader has no NVRHI binding layout.");
        }
        m_impl->vertex_shader = shaders.vertex_shader;
        m_impl->pixel_shader = shaders.pixel_shader;
        m_impl->binding_layout = shaders.binding_layouts.front();
        m_impl->reflection = program.reflection;
        m_impl->sampler = context.device().createSampler(nvrhi::SamplerDesc{});
        if (!m_impl->sampler) {
            throw std::runtime_error("NVRHI failed to create the MRT composite sampler.");
        }
    }

    const std::array resources = {
            renderer::vulkan::NvrhiBindingResource::Texture(
                    "color_output", m_impl->source->colorOutput()),
            renderer::vulkan::NvrhiBindingResource::Texture(
                    "auxiliary_output", m_impl->source->auxiliaryOutput()),
            renderer::vulkan::NvrhiBindingResource::Sampler(
                    "output_sampler", *m_impl->sampler),
    };
    m_impl->binding_set = renderer::vulkan::createNvrhiBindingSet(
            context.device(), m_impl->reflection, 0, *m_impl->binding_layout, resources);

    if (!m_impl->pipeline) {
        nvrhi::DepthStencilState depth_state;
        depth_state.disableDepthTest().disableDepthWrite().disableStencil();
        nvrhi::RenderState render_state;
        render_state.setDepthStencilState(depth_state);
        nvrhi::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.setPrimType(nvrhi::PrimitiveType::TriangleList)
                .setVertexShader(m_impl->vertex_shader)
                .setPixelShader(m_impl->pixel_shader)
                .setRenderState(render_state)
                .addBindingLayout(m_impl->binding_layout);
        m_impl->pipeline = context.device().createGraphicsPipeline(
                pipeline_desc, context.framebuffer().getFramebufferInfo());
        if (!m_impl->pipeline) {
            throw std::runtime_error("NVRHI failed to create the MRT composite pipeline.");
        }
    }
}

void MrtCompositePass::record(renderer::RenderPassContext& context)
{
    if (!m_impl->pipeline || !m_impl->binding_set) {
        throw std::logic_error("MrtCompositePass was not prepared.");
    }

    auto& commands = context.commands();
    commands.clearTextureFloat(&context.colorTexture(), nvrhi::AllSubresources,
            nvrhi::Color{0.02f, 0.025f, 0.03f, 1.0f});
    nvrhi::ViewportState viewport;
    viewport.addViewportAndScissorRect(context.framebufferInfo().getViewport());
    nvrhi::GraphicsState state;
    state.setPipeline(m_impl->pipeline)
            .setFramebuffer(&context.framebuffer())
            .setViewport(viewport)
            .addBindingSet(m_impl->binding_set);
    commands.setGraphicsState(state);
    commands.draw(nvrhi::DrawArguments{}.setVertexCount(3));
}

} // namespace arti::test_app
