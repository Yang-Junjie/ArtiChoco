#include "triangle_pass.h"

#include "artichoco/renderer/render_device.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"

#include <nvrhi/nvrhi.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>

namespace arti::hello_triangle {
namespace {

struct TriangleVertex {
    float position[2];
    float color[3];
};

constexpr std::array kVertices = {
    TriangleVertex{ { 0.0f, 0.72f }, { 0.95f, 0.25f, 0.18f } },
    TriangleVertex{ { -0.72f, -0.62f }, { 0.18f, 0.48f, 0.96f } },
    TriangleVertex{ { 0.72f, -0.62f }, { 0.20f, 0.82f, 0.42f } },
};

renderer::VertexBufferLayout triangleVertexLayout() {
    renderer::VertexBufferLayout layout;
    layout.stride = sizeof(TriangleVertex);
    layout.attributes = {
        { 0, renderer::VertexAttributeType::Float2, offsetof(TriangleVertex, position) },
        { 1, renderer::VertexAttributeType::Float3, offsetof(TriangleVertex, color) },
    };
    return layout;
}

} // namespace

struct TrianglePass::Impl {
    Impl(renderer::RenderDevice& device, std::filesystem::path path)
            : shader_path(std::move(path)),
              vertex_buffer(device.createVertexBuffer(std::as_bytes(std::span{ kVertices }),
                      static_cast<uint32_t>(kVertices.size()), triangleVertexLayout())) {}

    std::filesystem::path shader_path;
    renderer::VertexBuffer vertex_buffer;
    nvrhi::ShaderHandle vertex_shader;
    nvrhi::ShaderHandle pixel_shader;
    nvrhi::InputLayoutHandle input_layout;
    nvrhi::GraphicsPipelineHandle pipeline;
};

TrianglePass::TrianglePass(renderer::RenderDevice& device, std::filesystem::path shader_path)
        : m_impl(std::make_unique<Impl>(device, std::move(shader_path))) {}

TrianglePass::~TrianglePass() = default;

void TrianglePass::prepare(renderer::RenderPassPrepareContext& context) {
    if (m_impl->pipeline) {
        return;
    }

    const renderer::CompiledGraphicsProgram program =
            renderer::SlangCompiler::compileGraphics({ m_impl->shader_path });
    const auto shaders = renderer::vulkan::createNvrhiGraphicsShaderSet(context.device(), program,
            "Hello Triangle");
    m_impl->vertex_shader = shaders.vertex_shader;
    m_impl->pixel_shader = shaders.pixel_shader;

    nvrhi::VertexAttributeDesc position;
    position.setName("POSITION")
            .setFormat(nvrhi::Format::RG32_FLOAT)
            .setBufferIndex(0)
            .setOffset(offsetof(TriangleVertex, position))
            .setElementStride(sizeof(TriangleVertex));
    nvrhi::VertexAttributeDesc color;
    color.setName("COLOR0")
            .setFormat(nvrhi::Format::RGB32_FLOAT)
            .setBufferIndex(0)
            .setOffset(offsetof(TriangleVertex, color))
            .setElementStride(sizeof(TriangleVertex));
    const std::array attributes = { position, color };
    m_impl->input_layout = context.device().createInputLayout(attributes.data(), attributes.size(),
            m_impl->vertex_shader);
    if (!m_impl->input_layout) {
        throw std::runtime_error("NVRHI failed to create the triangle input layout.");
    }

    nvrhi::DepthStencilState depth_state;
    depth_state.disableDepthTest().disableDepthWrite().disableStencil();
    nvrhi::RasterState raster_state;
    raster_state.setCullNone();
    nvrhi::RenderState render_state;
    render_state.setDepthStencilState(depth_state).setRasterState(raster_state);

    nvrhi::GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.setPrimType(nvrhi::PrimitiveType::TriangleList)
            .setInputLayout(m_impl->input_layout)
            .setVertexShader(m_impl->vertex_shader)
            .setPixelShader(m_impl->pixel_shader)
            .setRenderState(render_state);
    for (const nvrhi::BindingLayoutHandle& layout: shaders.binding_layouts) {
        if (layout) {
            pipeline_desc.addBindingLayout(layout);
        }
    }
    m_impl->pipeline =
            context.device().createGraphicsPipeline(pipeline_desc, context.framebufferInfo());
    if (!m_impl->pipeline) {
        throw std::runtime_error("NVRHI failed to create the triangle graphics pipeline.");
    }
}

void TrianglePass::record(renderer::RenderPassContext& context) {
    if (!m_impl->pipeline) {
        throw std::logic_error("TrianglePass was not prepared.");
    }

    auto& commands = context.commands();
    commands.clearTextureFloat(&context.colorTexture(), nvrhi::AllSubresources,
            nvrhi::Color{ 0.018f, 0.026f, 0.04f, 1.0f });

    nvrhi::ViewportState viewport;
    viewport.addViewportAndScissorRect(context.framebufferInfo().getViewport());
    nvrhi::GraphicsState state;
    state.setPipeline(m_impl->pipeline)
            .setFramebuffer(&context.framebuffer())
            .setViewport(viewport)
            .addVertexBuffer(nvrhi::VertexBufferBinding()
                            .setBuffer(&context.buffer(m_impl->vertex_buffer))
                            .setSlot(0));
    commands.setGraphicsState(state);
    commands.draw(nvrhi::DrawArguments{}.setVertexCount(m_impl->vertex_buffer.vertexCount()));
}

} // namespace arti::hello_triangle
