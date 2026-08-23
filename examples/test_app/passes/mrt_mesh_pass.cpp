#include "mrt_mesh_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"

#include <array>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace arti::test_app {
namespace {

struct MeshFrameUniforms {
    std::array<float, 16> model_view_projection;
    std::array<float, 4> tint;
};

static_assert(std::is_standard_layout_v<MeshFrameUniforms>);
static_assert(sizeof(MeshFrameUniforms) == sizeof(float) * 20);

constexpr std::array<float, 4> meshMaterialData = { 1.0f, 0.94f, 0.86f, 1.0f };

nvrhi::Format toNvrhiVertexFormat(renderer::VertexAttributeType type) {
    switch (type) {
        case renderer::VertexAttributeType::Float2:
            return nvrhi::Format::RG32_FLOAT;
        case renderer::VertexAttributeType::Float3:
            return nvrhi::Format::RGB32_FLOAT;
        case renderer::VertexAttributeType::Float4:
            return nvrhi::Format::RGBA32_FLOAT;
    }
    throw std::invalid_argument("Unsupported NVRHI vertex attribute type.");
}

nvrhi::Format toNvrhiIndexFormat(renderer::IndexType type) {
    switch (type) {
        case renderer::IndexType::UInt16:
            return nvrhi::Format::R16_UINT;
        case renderer::IndexType::UInt32:
            return nvrhi::Format::R32_UINT;
    }
    throw std::invalid_argument("Unsupported NVRHI index type.");
}

const char* semanticName(uint32_t location) {
    switch (location) {
        case 0:
            return "POSITION";
        case 1:
            return "TEXCOORD0";
        default:
            throw std::invalid_argument("MrtMeshPass supports POSITION and TEXCOORD0 only.");
    }
}

} // namespace

struct MrtMeshPass::Impl {
    Impl(TextureComputePass& texture_source, std::filesystem::path shader_path)
            : texture_source(&texture_source),
              shader_path(std::move(shader_path)) {}

    TextureComputePass* texture_source{ nullptr };
    std::filesystem::path shader_path;
    const RenderFrameData* frame_data{ nullptr };
    std::array<float, 4> clear_color{ 0.04f, 0.08f, 0.12f, 1.0f };

    renderer::ShaderReflection reflection;
    nvrhi::ShaderHandle vertex_shader;
    nvrhi::ShaderHandle pixel_shader;
    nvrhi::BindingLayoutHandle binding_layout;
    nvrhi::BindingSetHandle binding_set;
    nvrhi::InputLayoutHandle input_layout;
    nvrhi::GraphicsPipelineHandle pipeline;
    nvrhi::SamplerHandle sampler;
    nvrhi::BufferHandle material_buffer;
    nvrhi::TextureHandle color_output;
    nvrhi::TextureHandle auxiliary_output;
    nvrhi::TextureHandle depth_buffer;
    nvrhi::FramebufferHandle framebuffer;
};

MrtMeshPass::MrtMeshPass(TextureComputePass& texture_source,
        const std::filesystem::path& shader_path)
        : m_impl(std::make_unique<Impl>(texture_source, shader_path)) {}

MrtMeshPass::~MrtMeshPass() = default;

void MrtMeshPass::applyFrameData(const RenderFrameData& frame_data) {
    m_impl->frame_data = &frame_data;
}

void MrtMeshPass::setClearColor(const std::array<float, 4>& color) noexcept {
    m_impl->clear_color = color;
}

void MrtMeshPass::prepare(renderer::RenderPassPrepareContext& context) {
    if (!m_impl->texture_source) {
        throw std::logic_error("MrtMeshPass requires a texture source.");
    }
    if (!m_impl->binding_layout) {
        const renderer::CompiledGraphicsProgram program =
                renderer::SlangCompiler::compileGraphics({ m_impl->shader_path });
        const auto shaders = renderer::vulkan::createNvrhiGraphicsShaderSet(context.device(),
                program, "ArtiChoco NVRHI MRT mesh");
        if (shaders.binding_layouts.empty() || !shaders.binding_layouts.front()) {
            throw std::runtime_error("MrtMeshPass shader has no NVRHI binding layout.");
        }
        m_impl->vertex_shader = shaders.vertex_shader;
        m_impl->pixel_shader = shaders.pixel_shader;
        m_impl->binding_layout = shaders.binding_layouts.front();
        m_impl->reflection = program.reflection;

        nvrhi::BufferDesc material_desc;
        material_desc.setByteSize(sizeof(meshMaterialData))
                .setStructStride(sizeof(float) * 4)
                .setDebugName("ArtiChoco NVRHI MRT material")
                .enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource);
        m_impl->material_buffer = context.device().createBuffer(material_desc);
        m_impl->sampler = context.device().createSampler(nvrhi::SamplerDesc{});
        if (!m_impl->material_buffer || !m_impl->sampler) {
            throw std::runtime_error("NVRHI failed to create MRT mesh resources.");
        }
    }

    auto& source = m_impl->texture_source->output();
    const uint32_t width = source.getDesc().width;
    const uint32_t height = source.getDesc().height;
    const bool size_changed = !m_impl->color_output ||
                              m_impl->color_output->getDesc().width != width ||
                              m_impl->color_output->getDesc().height != height;
    if (size_changed) {
        nvrhi::TextureDesc color_desc;
        color_desc.setWidth(width)
                .setHeight(height)
                .setFormat(nvrhi::Format::RGBA8_UNORM)
                .setIsRenderTarget(true)
                .setDebugName("ArtiChoco NVRHI MRT color")
                .enableAutomaticStateTracking(nvrhi::ResourceStates::ShaderResource);
        nvrhi::TextureDesc auxiliary_desc = color_desc;
        auxiliary_desc.setFormat(nvrhi::Format::RGBA16_FLOAT)
                .setDebugName("ArtiChoco NVRHI MRT auxiliary");
        nvrhi::TextureDesc depth_desc;
        depth_desc.setWidth(width)
                .setHeight(height)
                .setFormat(nvrhi::Format::D32)
                .setIsRenderTarget(true)
                .setDebugName("ArtiChoco NVRHI MRT depth")
                .enableAutomaticStateTracking(nvrhi::ResourceStates::DepthWrite);

        m_impl->color_output = context.device().createTexture(color_desc);
        m_impl->auxiliary_output = context.device().createTexture(auxiliary_desc);
        m_impl->depth_buffer = context.device().createTexture(depth_desc);
        if (!m_impl->color_output || !m_impl->auxiliary_output || !m_impl->depth_buffer) {
            throw std::runtime_error("NVRHI failed to create MRT attachments.");
        }
        nvrhi::FramebufferDesc framebuffer_desc;
        framebuffer_desc.addColorAttachment(m_impl->color_output)
                .addColorAttachment(m_impl->auxiliary_output)
                .setDepthAttachment(m_impl->depth_buffer);
        m_impl->framebuffer = context.device().createFramebuffer(framebuffer_desc);
        if (!m_impl->framebuffer) {
            throw std::runtime_error("NVRHI failed to create the MRT framebuffer.");
        }
        m_impl->pipeline = nullptr;
        m_impl->input_layout = nullptr;
    }

    const std::array resources = {
        renderer::vulkan::NvrhiBindingResource::Buffer("material_data", *m_impl->material_buffer),
        renderer::vulkan::NvrhiBindingResource::Texture("base_color_texture", source),
        renderer::vulkan::NvrhiBindingResource::Sampler("base_color_sampler", *m_impl->sampler),
    };
    m_impl->binding_set = renderer::vulkan::createNvrhiBindingSet(context.device(),
            m_impl->reflection, 0, *m_impl->binding_layout, resources);
}

void MrtMeshPass::record(renderer::RenderPassContext& context) {
    if (!m_impl->frame_data || m_impl->frame_data->draws.empty()) {
        return;
    }
    const auto& frame_data = *m_impl->frame_data;
    const auto& framebuffer_info = m_impl->framebuffer->getFramebufferInfo();
    auto& commands = context.commands();
    commands.clearTextureFloat(m_impl->color_output, nvrhi::AllSubresources,
            nvrhi::Color{ m_impl->clear_color[0], m_impl->clear_color[1], m_impl->clear_color[2],
                m_impl->clear_color[3] });
    commands.clearTextureFloat(m_impl->auxiliary_output, nvrhi::AllSubresources,
            nvrhi::Color{ 0.08f, 0.12f, 0.16f, 1.0f });
    commands.clearDepthStencilTexture(m_impl->depth_buffer, nvrhi::AllSubresources, true, 1.0f,
            false, 0);
    commands.writeBuffer(m_impl->material_buffer, meshMaterialData.data(),
            sizeof(meshMaterialData));

    for (const auto& draw: frame_data.draws) {
        if (!draw.vertex_buffer || !draw.index_buffer) {
            continue;
        }
        if (!m_impl->pipeline) {
            const auto& layout = draw.vertex_buffer->layout();
            std::vector<nvrhi::VertexAttributeDesc> attributes;
            attributes.reserve(layout.attributes.size());
            for (const auto& attribute: layout.attributes) {
                nvrhi::VertexAttributeDesc desc;
                desc.setName(semanticName(attribute.location))
                        .setFormat(toNvrhiVertexFormat(attribute.type))
                        .setBufferIndex(0)
                        .setOffset(attribute.offset)
                        .setElementStride(layout.stride);
                attributes.push_back(desc);
            }
            m_impl->input_layout = context.device().createInputLayout(attributes.data(),
                    attributes.size(), m_impl->vertex_shader);
            if (!m_impl->input_layout) {
                throw std::runtime_error("NVRHI failed to create the MRT mesh input layout.");
            }

            nvrhi::DepthStencilState depth_state;
            depth_state.enableDepthTest().enableDepthWrite().disableStencil();
            nvrhi::RasterState raster_state;
            raster_state.setCullBack().setFrontCounterClockwise(true);
            nvrhi::RenderState render_state;
            render_state.setDepthStencilState(depth_state).setRasterState(raster_state);
            nvrhi::GraphicsPipelineDesc pipeline_desc;
            pipeline_desc.setPrimType(nvrhi::PrimitiveType::TriangleList)
                    .setInputLayout(m_impl->input_layout)
                    .setVertexShader(m_impl->vertex_shader)
                    .setPixelShader(m_impl->pixel_shader)
                    .setRenderState(render_state)
                    .addBindingLayout(m_impl->binding_layout);
            m_impl->pipeline =
                    context.device().createGraphicsPipeline(pipeline_desc, framebuffer_info);
            if (!m_impl->pipeline) {
                throw std::runtime_error("NVRHI failed to create the MRT mesh pipeline.");
            }
        }

        MeshFrameUniforms frame_uniforms{};
        const glm::mat4 mvp = frame_data.projection * frame_data.view * draw.model_matrix;
        std::memcpy(frame_uniforms.model_view_projection.data(), glm::value_ptr(mvp),
                sizeof(frame_uniforms.model_view_projection));
        frame_uniforms.tint = { 1.0f, 1.0f, 1.0f, 1.0f };

        nvrhi::ViewportState viewport;
        viewport.addViewportAndScissorRect(framebuffer_info.getViewport());
        nvrhi::GraphicsState state;
        state.setPipeline(m_impl->pipeline)
                .setFramebuffer(m_impl->framebuffer)
                .setViewport(viewport)
                .addBindingSet(m_impl->binding_set)
                .addVertexBuffer(nvrhi::VertexBufferBinding()
                                .setBuffer(&context.buffer(*draw.vertex_buffer))
                                .setSlot(0))
                .setIndexBuffer(nvrhi::IndexBufferBinding()
                                .setBuffer(&context.buffer(*draw.index_buffer))
                                .setFormat(toNvrhiIndexFormat(draw.index_buffer->indexType())));
        commands.setGraphicsState(state);
        commands.setPushConstants(&frame_uniforms, sizeof(frame_uniforms));
        commands.drawIndexed(
                nvrhi::DrawArguments{}.setVertexCount(draw.index_buffer->indexCount()));
    }
}

nvrhi::ITexture& MrtMeshPass::colorOutput() const {
    if (!m_impl->color_output) {
        throw std::logic_error("MrtMeshPass color output is not initialized.");
    }
    return *m_impl->color_output;
}

nvrhi::ITexture& MrtMeshPass::auxiliaryOutput() const {
    if (!m_impl->auxiliary_output) {
        throw std::logic_error("MrtMeshPass auxiliary output is not initialized.");
    }
    return *m_impl->auxiliary_output;
}

} // namespace arti::test_app
