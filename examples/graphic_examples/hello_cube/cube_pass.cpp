#include "cube_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/render_device.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"

#include <nvrhi/nvrhi.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace arti::hello_cube {
namespace {

struct CubeVertex {
    glm::vec3 position;
    glm::vec2 uv;
};

struct CubePushConstants {
    std::array<float, 16> model_view_projection;
};

static_assert(std::is_standard_layout_v<CubeVertex>);
static_assert(std::is_standard_layout_v<CubePushConstants>);
static_assert(sizeof(CubePushConstants) == sizeof(float) * 16);

constexpr std::array kVertices = {
    CubeVertex{ { -0.8f, -0.8f, 0.8f }, { 0.0f, 1.0f } },
    CubeVertex{ { 0.8f, -0.8f, 0.8f }, { 1.0f, 1.0f } },
    CubeVertex{ { 0.8f, 0.8f, 0.8f }, { 1.0f, 0.0f } },
    CubeVertex{ { -0.8f, 0.8f, 0.8f }, { 0.0f, 0.0f } },

    CubeVertex{ { 0.8f, -0.8f, -0.8f }, { 0.0f, 1.0f } },
    CubeVertex{ { -0.8f, -0.8f, -0.8f }, { 1.0f, 1.0f } },
    CubeVertex{ { -0.8f, 0.8f, -0.8f }, { 1.0f, 0.0f } },
    CubeVertex{ { 0.8f, 0.8f, -0.8f }, { 0.0f, 0.0f } },

    CubeVertex{ { 0.8f, -0.8f, 0.8f }, { 0.0f, 1.0f } },
    CubeVertex{ { 0.8f, -0.8f, -0.8f }, { 1.0f, 1.0f } },
    CubeVertex{ { 0.8f, 0.8f, -0.8f }, { 1.0f, 0.0f } },
    CubeVertex{ { 0.8f, 0.8f, 0.8f }, { 0.0f, 0.0f } },

    CubeVertex{ { -0.8f, -0.8f, -0.8f }, { 0.0f, 1.0f } },
    CubeVertex{ { -0.8f, -0.8f, 0.8f }, { 1.0f, 1.0f } },
    CubeVertex{ { -0.8f, 0.8f, 0.8f }, { 1.0f, 0.0f } },
    CubeVertex{ { -0.8f, 0.8f, -0.8f }, { 0.0f, 0.0f } },

    CubeVertex{ { -0.8f, 0.8f, 0.8f }, { 0.0f, 1.0f } },
    CubeVertex{ { 0.8f, 0.8f, 0.8f }, { 1.0f, 1.0f } },
    CubeVertex{ { 0.8f, 0.8f, -0.8f }, { 1.0f, 0.0f } },
    CubeVertex{ { -0.8f, 0.8f, -0.8f }, { 0.0f, 0.0f } },

    CubeVertex{ { -0.8f, -0.8f, -0.8f }, { 0.0f, 1.0f } },
    CubeVertex{ { 0.8f, -0.8f, -0.8f }, { 1.0f, 1.0f } },
    CubeVertex{ { 0.8f, -0.8f, 0.8f }, { 1.0f, 0.0f } },
    CubeVertex{ { -0.8f, -0.8f, 0.8f }, { 0.0f, 0.0f } },
};

constexpr std::array<uint32_t, 36> kIndices = {
    0,
    1,
    2,
    2,
    3,
    0,
    4,
    5,
    6,
    6,
    7,
    4,
    8,
    9,
    10,
    10,
    11,
    8,
    12,
    13,
    14,
    14,
    15,
    12,
    16,
    17,
    18,
    18,
    19,
    16,
    20,
    21,
    22,
    22,
    23,
    20,
};

renderer::VertexBufferLayout cubeVertexLayout() {
    renderer::VertexBufferLayout layout;
    layout.stride = sizeof(CubeVertex);
    layout.attributes = {
        { 0, renderer::VertexAttributeType::Float3, offsetof(CubeVertex, position) },
        { 1, renderer::VertexAttributeType::Float2, offsetof(CubeVertex, uv) },
    };
    return layout;
}

nvrhi::Format toNvrhiIndexFormat(renderer::IndexType type) {
    switch (type) {
        case renderer::IndexType::UInt16:
            return nvrhi::Format::R16_UINT;
        case renderer::IndexType::UInt32:
            return nvrhi::Format::R32_UINT;
    }
    throw std::invalid_argument("Unsupported cube index format.");
}

} // namespace

struct CubePass::Impl {
    Impl(renderer::RenderDevice& device, renderer::Texture2D source_texture,
            std::filesystem::path path)
            : shader_path(std::move(path)),
              vertex_buffer(device.createVertexBuffer(std::as_bytes(std::span{ kVertices }),
                      static_cast<uint32_t>(kVertices.size()), cubeVertexLayout())),
              index_buffer(device.createIndexBuffer(std::as_bytes(std::span{ kIndices }),
                      static_cast<uint32_t>(kIndices.size()))),
              texture(std::move(source_texture)) {}

    std::filesystem::path shader_path;
    renderer::VertexBuffer vertex_buffer;
    renderer::IndexBuffer index_buffer;
    renderer::Texture2D texture;
    float rotation{ 0.0f };
    renderer::ShaderReflection reflection;
    nvrhi::ShaderHandle vertex_shader;
    nvrhi::ShaderHandle pixel_shader;
    nvrhi::BindingLayoutHandle binding_layout;
    nvrhi::BindingSetHandle binding_set;
    nvrhi::InputLayoutHandle input_layout;
    nvrhi::GraphicsPipelineHandle pipeline;
    nvrhi::SamplerHandle sampler;
};

CubePass::CubePass(renderer::RenderDevice& device, renderer::Texture2D texture,
        std::filesystem::path shader_path)
        : m_impl(std::make_unique<Impl>(device, std::move(texture), std::move(shader_path))) {}

CubePass::~CubePass() = default;

void CubePass::setRotation(float radians) noexcept { m_impl->rotation = radians; }

void CubePass::prepare(renderer::RenderPassPrepareContext& context) {
    if (m_impl->pipeline) {
        return;
    }

    const renderer::CompiledGraphicsProgram program =
            renderer::SlangCompiler::compileGraphics({ m_impl->shader_path });
    const auto shaders =
            renderer::vulkan::createNvrhiGraphicsShaderSet(context.device(), program, "Hello Cube");
    if (shaders.binding_layouts.size() != 1 || !shaders.binding_layouts.front()) {
        throw std::runtime_error("The cube shader requires one NVRHI binding layout.");
    }
    m_impl->reflection = program.reflection;
    m_impl->vertex_shader = shaders.vertex_shader;
    m_impl->pixel_shader = shaders.pixel_shader;
    m_impl->binding_layout = shaders.binding_layouts.front();

    nvrhi::VertexAttributeDesc position;
    position.setName("POSITION")
            .setFormat(nvrhi::Format::RGB32_FLOAT)
            .setBufferIndex(0)
            .setOffset(offsetof(CubeVertex, position))
            .setElementStride(sizeof(CubeVertex));
    nvrhi::VertexAttributeDesc uv;
    uv.setName("TEXCOORD0")
            .setFormat(nvrhi::Format::RG32_FLOAT)
            .setBufferIndex(0)
            .setOffset(offsetof(CubeVertex, uv))
            .setElementStride(sizeof(CubeVertex));
    const std::array attributes = { position, uv };
    m_impl->input_layout = context.device().createInputLayout(attributes.data(), attributes.size(),
            m_impl->vertex_shader);

    nvrhi::SamplerDesc sampler_desc;
    sampler_desc.setAllFilters(true).setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
    m_impl->sampler = context.device().createSampler(sampler_desc);

    nvrhi::DepthStencilState depth_state;
    depth_state.disableDepthTest().disableDepthWrite().disableStencil();
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
            context.device().createGraphicsPipeline(pipeline_desc, context.framebufferInfo());
    if (!m_impl->input_layout || !m_impl->sampler || !m_impl->pipeline) {
        throw std::runtime_error("NVRHI failed to create Hello Cube pipeline resources.");
    }
}

void CubePass::record(renderer::RenderPassContext& context) {
    if (!m_impl->pipeline || !m_impl->binding_layout || !m_impl->sampler) {
        throw std::logic_error("CubePass was not prepared.");
    }

    if (!m_impl->binding_set) {
        const std::array resources = {
            renderer::vulkan::NvrhiBindingResource::Texture("base_color_texture",
                    context.texture(m_impl->texture)),
            renderer::vulkan::NvrhiBindingResource::Sampler("base_color_sampler", *m_impl->sampler),
        };
        m_impl->binding_set = renderer::vulkan::createNvrhiBindingSet(context.device(),
                m_impl->reflection, 0, *m_impl->binding_layout, resources);
    }

    const auto& framebuffer_info = context.framebufferInfo();
    const float aspect = framebuffer_info.height == 0
                                 ? 1.0f
                                 : static_cast<float>(framebuffer_info.width) /
                                           static_cast<float>(framebuffer_info.height);
    glm::mat4 model{ 1.0f };
    model = glm::rotate(model, m_impl->rotation, glm::vec3{ 0.0f, 1.0f, 0.0f });
    model = glm::rotate(model, m_impl->rotation * 0.55f, glm::vec3{ 1.0f, 0.0f, 0.0f });
    const glm::mat4 view = glm::lookAt(glm::vec3{ 2.7f, 2.1f, 3.0f }, glm::vec3{ 0.0f },
            glm::vec3{ 0.0f, 1.0f, 0.0f });
    const glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    const glm::mat4 mvp = projection * view * model;
    CubePushConstants push_constants{};
    std::memcpy(push_constants.model_view_projection.data(), glm::value_ptr(mvp), sizeof(mvp));

    auto& commands = context.commands();
    commands.clearTextureFloat(&context.colorTexture(), nvrhi::AllSubresources,
            nvrhi::Color{ 0.025f, 0.035f, 0.055f, 1.0f });
    nvrhi::ViewportState viewport;
    viewport.addViewportAndScissorRect(framebuffer_info.getViewport());
    nvrhi::GraphicsState state;
    state.setPipeline(m_impl->pipeline)
            .setFramebuffer(&context.framebuffer())
            .setViewport(viewport)
            .addBindingSet(m_impl->binding_set)
            .addVertexBuffer(nvrhi::VertexBufferBinding()
                            .setBuffer(&context.buffer(m_impl->vertex_buffer))
                            .setSlot(0))
            .setIndexBuffer(nvrhi::IndexBufferBinding()
                            .setBuffer(&context.buffer(m_impl->index_buffer))
                            .setFormat(toNvrhiIndexFormat(m_impl->index_buffer.indexType())));
    commands.setGraphicsState(state);
    commands.setPushConstants(&push_constants, sizeof(push_constants));
    commands.drawIndexed(nvrhi::DrawArguments{}.setVertexCount(m_impl->index_buffer.indexCount()));
}

} // namespace arti::hello_cube
