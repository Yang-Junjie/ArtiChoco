#include "instancing_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/render_device.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace arti::instancing {
namespace {

struct CubeVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct InstanceData {
    glm::vec4 translation_and_scale;
    glm::vec4 color;
};

struct InstancingPushConstants {
    std::array<float, 16> view_projection;
    std::array<float, 4> animation;
};

static_assert(std::is_standard_layout_v<CubeVertex>);
static_assert(std::is_standard_layout_v<InstanceData>);
static_assert(std::is_standard_layout_v<InstancingPushConstants>);
static_assert(sizeof(InstanceData) == sizeof(float) * 8);
static_assert(sizeof(InstancingPushConstants) == sizeof(float) * 20);

constexpr float kCubeExtent = 0.5f;
constexpr std::array kVertices = {
    CubeVertex{ { -kCubeExtent, -kCubeExtent, kCubeExtent }, { 0.0f, 0.0f, 1.0f } },
    CubeVertex{ { kCubeExtent, -kCubeExtent, kCubeExtent }, { 0.0f, 0.0f, 1.0f } },
    CubeVertex{ { kCubeExtent, kCubeExtent, kCubeExtent }, { 0.0f, 0.0f, 1.0f } },
    CubeVertex{ { -kCubeExtent, kCubeExtent, kCubeExtent }, { 0.0f, 0.0f, 1.0f } },

    CubeVertex{ { kCubeExtent, -kCubeExtent, -kCubeExtent }, { 0.0f, 0.0f, -1.0f } },
    CubeVertex{ { -kCubeExtent, -kCubeExtent, -kCubeExtent }, { 0.0f, 0.0f, -1.0f } },
    CubeVertex{ { -kCubeExtent, kCubeExtent, -kCubeExtent }, { 0.0f, 0.0f, -1.0f } },
    CubeVertex{ { kCubeExtent, kCubeExtent, -kCubeExtent }, { 0.0f, 0.0f, -1.0f } },

    CubeVertex{ { kCubeExtent, -kCubeExtent, kCubeExtent }, { 1.0f, 0.0f, 0.0f } },
    CubeVertex{ { kCubeExtent, -kCubeExtent, -kCubeExtent }, { 1.0f, 0.0f, 0.0f } },
    CubeVertex{ { kCubeExtent, kCubeExtent, -kCubeExtent }, { 1.0f, 0.0f, 0.0f } },
    CubeVertex{ { kCubeExtent, kCubeExtent, kCubeExtent }, { 1.0f, 0.0f, 0.0f } },

    CubeVertex{ { -kCubeExtent, -kCubeExtent, -kCubeExtent }, { -1.0f, 0.0f, 0.0f } },
    CubeVertex{ { -kCubeExtent, -kCubeExtent, kCubeExtent }, { -1.0f, 0.0f, 0.0f } },
    CubeVertex{ { -kCubeExtent, kCubeExtent, kCubeExtent }, { -1.0f, 0.0f, 0.0f } },
    CubeVertex{ { -kCubeExtent, kCubeExtent, -kCubeExtent }, { -1.0f, 0.0f, 0.0f } },

    CubeVertex{ { -kCubeExtent, kCubeExtent, kCubeExtent }, { 0.0f, 1.0f, 0.0f } },
    CubeVertex{ { kCubeExtent, kCubeExtent, kCubeExtent }, { 0.0f, 1.0f, 0.0f } },
    CubeVertex{ { kCubeExtent, kCubeExtent, -kCubeExtent }, { 0.0f, 1.0f, 0.0f } },
    CubeVertex{ { -kCubeExtent, kCubeExtent, -kCubeExtent }, { 0.0f, 1.0f, 0.0f } },

    CubeVertex{ { -kCubeExtent, -kCubeExtent, -kCubeExtent }, { 0.0f, -1.0f, 0.0f } },
    CubeVertex{ { kCubeExtent, -kCubeExtent, -kCubeExtent }, { 0.0f, -1.0f, 0.0f } },
    CubeVertex{ { kCubeExtent, -kCubeExtent, kCubeExtent }, { 0.0f, -1.0f, 0.0f } },
    CubeVertex{ { -kCubeExtent, -kCubeExtent, kCubeExtent }, { 0.0f, -1.0f, 0.0f } },
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

constexpr size_t kGridWidth = 10;
constexpr size_t kInstanceCount = kGridWidth * kGridWidth * kGridWidth;
constexpr float kGridSpacing = 1.15f;
constexpr float kGridCenter = static_cast<float>(kGridWidth - 1) * 0.5f;
constexpr float kMaximumCubeScale = 0.60f;
constexpr float kMaximumBobOffset = 0.16f;
constexpr float kVerticalFieldOfViewDegrees = 42.0f;
constexpr float kCameraFitMargin = 1.08f;

static_assert(kGridWidth > 1);

constexpr std::array<InstanceData, kInstanceCount> makeInstances() {
    std::array<InstanceData, kInstanceCount> instances{};
    for (size_t y = 0; y < kGridWidth; ++y) {
        for (size_t z = 0; z < kGridWidth; ++z) {
            for (size_t x = 0; x < kGridWidth; ++x) {
                const size_t index = (y * kGridWidth + z) * kGridWidth + x;
                const float normalized_x =
                        static_cast<float>(x) / static_cast<float>(kGridWidth - 1);
                const float normalized_y =
                        static_cast<float>(y) / static_cast<float>(kGridWidth - 1);
                const float normalized_z =
                        static_cast<float>(z) / static_cast<float>(kGridWidth - 1);
                const glm::vec3 position = (glm::vec3{ static_cast<float>(x), static_cast<float>(y),
                                                static_cast<float>(z) } -
                                                   kGridCenter) *
                                           kGridSpacing;
                const float scale =
                        0.42f + static_cast<float>((x * 5 + y * 7 + z * 3) % 5) * 0.045f;
                instances[index] = {
                    glm::vec4{ position, scale },
                    glm::vec4{ 0.16f + normalized_x * 0.72f, 0.22f + normalized_y * 0.65f,
                        0.25f + normalized_z * 0.68f, 1.0f },
                };
            }
        }
    }
    return instances;
}

const auto kInstances = makeInstances();

renderer::VertexBufferLayout cubeVertexLayout() {
    renderer::VertexBufferLayout layout;
    layout.stride = sizeof(CubeVertex);
    layout.attributes = {
        { 0, renderer::VertexAttributeType::Float3, offsetof(CubeVertex, position) },
        { 1, renderer::VertexAttributeType::Float3, offsetof(CubeVertex, normal) },
    };
    return layout;
}

renderer::VertexBufferLayout instanceVertexLayout() {
    renderer::VertexBufferLayout layout;
    layout.stride = sizeof(InstanceData);
    layout.attributes = {
        { 2, renderer::VertexAttributeType::Float4, offsetof(InstanceData, translation_and_scale) },
        { 3, renderer::VertexAttributeType::Float4, offsetof(InstanceData, color) },
    };
    return layout;
}

} // namespace

struct InstancingPass::Impl {
    struct BackBufferTarget {
        nvrhi::TextureHandle depth_buffer;
        nvrhi::FramebufferHandle framebuffer;
    };

    Impl(renderer::RenderDevice& device, std::filesystem::path path)
            : shader_path(std::move(path)),
              vertex_buffer(device.createVertexBuffer(std::as_bytes(std::span{ kVertices }),
                      static_cast<uint32_t>(kVertices.size()), cubeVertexLayout())),
              instance_buffer(device.createVertexBuffer(std::as_bytes(std::span{ kInstances }),
                      static_cast<uint32_t>(kInstances.size()), instanceVertexLayout())),
              index_buffer(device.createIndexBuffer(std::as_bytes(std::span{ kIndices }),
                      static_cast<uint32_t>(kIndices.size()))) {}

    std::filesystem::path shader_path;
    renderer::VertexBuffer vertex_buffer;
    renderer::VertexBuffer instance_buffer;
    renderer::IndexBuffer index_buffer;
    float time{ 0.0f };
    nvrhi::ShaderHandle vertex_shader;
    nvrhi::ShaderHandle pixel_shader;
    nvrhi::BindingLayoutHandle binding_layout;
    nvrhi::BindingSetHandle binding_set;
    nvrhi::InputLayoutHandle input_layout;
    nvrhi::GraphicsPipelineHandle pipeline;
    nvrhi::TextureHandle current_depth_buffer;
    nvrhi::FramebufferHandle current_framebuffer;
    std::unordered_map<nvrhi::ITexture*, BackBufferTarget> back_buffer_targets;
    uint32_t width{ 0 };
    uint32_t height{ 0 };
    nvrhi::Format color_format{ nvrhi::Format::UNKNOWN };
};

InstancingPass::InstancingPass(renderer::RenderDevice& device, std::filesystem::path shader_path)
        : m_impl(std::make_unique<Impl>(device, std::move(shader_path))) {}

InstancingPass::~InstancingPass() = default;

void InstancingPass::setTime(float seconds) noexcept { m_impl->time = seconds; }

void InstancingPass::prepare(renderer::RenderPassPrepareContext& context) {
    if (!m_impl->binding_layout) {
        const renderer::CompiledGraphicsProgram program =
                renderer::SlangCompiler::compileGraphics({ m_impl->shader_path });
        const auto shaders = renderer::vulkan::createNvrhiGraphicsShaderSet(context.device(),
                program, "Instancing");
        if (shaders.binding_layouts.size() != 1 || !shaders.binding_layouts.front()) {
            throw std::runtime_error("The instancing shader requires one binding layout.");
        }
        m_impl->vertex_shader = shaders.vertex_shader;
        m_impl->pixel_shader = shaders.pixel_shader;
        m_impl->binding_layout = shaders.binding_layouts.front();
        const std::array<renderer::vulkan::NvrhiBindingResource, 0> resources{};
        m_impl->binding_set = renderer::vulkan::createNvrhiBindingSet(context.device(),
                program.reflection, 0, *m_impl->binding_layout, resources);

        nvrhi::VertexAttributeDesc position;
        position.setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(CubeVertex, position))
                .setElementStride(sizeof(CubeVertex));
        nvrhi::VertexAttributeDesc normal;
        normal.setName("NORMAL")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(offsetof(CubeVertex, normal))
                .setElementStride(sizeof(CubeVertex));
        nvrhi::VertexAttributeDesc instance_transform;
        instance_transform.setName("INSTANCE_TRANSFORM")
                .setFormat(nvrhi::Format::RGBA32_FLOAT)
                .setBufferIndex(1)
                .setOffset(offsetof(InstanceData, translation_and_scale))
                .setElementStride(sizeof(InstanceData))
                .setIsInstanced(true);
        nvrhi::VertexAttributeDesc instance_color;
        instance_color.setName("INSTANCE_COLOR")
                .setFormat(nvrhi::Format::RGBA32_FLOAT)
                .setBufferIndex(1)
                .setOffset(offsetof(InstanceData, color))
                .setElementStride(sizeof(InstanceData))
                .setIsInstanced(true);
        const std::array attributes = { position, normal, instance_transform, instance_color };
        m_impl->input_layout = context.device().createInputLayout(attributes.data(),
                attributes.size(), m_impl->vertex_shader);
        if (!m_impl->input_layout) {
            throw std::runtime_error("NVRHI failed to create the instancing input layout.");
        }
    }

    const auto& framebuffer_info = context.framebufferInfo();
    const nvrhi::Format color_format = framebuffer_info.colorFormats.front();
    const bool framebuffer_changed = m_impl->width != framebuffer_info.width ||
                                     m_impl->height != framebuffer_info.height ||
                                     m_impl->color_format != color_format;
    if (framebuffer_changed) {
        m_impl->back_buffer_targets.clear();
        m_impl->current_depth_buffer = nullptr;
        m_impl->current_framebuffer = nullptr;
        m_impl->pipeline = nullptr;
        m_impl->width = framebuffer_info.width;
        m_impl->height = framebuffer_info.height;
        m_impl->color_format = color_format;
    }

    auto* color_texture = context.framebuffer().getDesc().colorAttachments.front().texture;
    auto [iterator, inserted] = m_impl->back_buffer_targets.try_emplace(color_texture);
    if (inserted) {
        nvrhi::TextureDesc depth_desc;
        depth_desc.setWidth(framebuffer_info.width)
                .setHeight(framebuffer_info.height)
                .setFormat(nvrhi::Format::D32)
                .setIsRenderTarget(true)
                .setDebugName("Instancing back-buffer depth")
                .enableAutomaticStateTracking(nvrhi::ResourceStates::DepthWrite);
        iterator->second.depth_buffer = context.device().createTexture(depth_desc);
        if (!iterator->second.depth_buffer) {
            throw std::runtime_error("NVRHI failed to create an instancing depth buffer.");
        }
        nvrhi::FramebufferDesc framebuffer_desc;
        framebuffer_desc.addColorAttachment(color_texture)
                .setDepthAttachment(iterator->second.depth_buffer);
        iterator->second.framebuffer = context.device().createFramebuffer(framebuffer_desc);
        if (!iterator->second.framebuffer) {
            throw std::runtime_error("NVRHI failed to create an instancing framebuffer.");
        }
    }
    m_impl->current_depth_buffer = iterator->second.depth_buffer;
    m_impl->current_framebuffer = iterator->second.framebuffer;

    if (!m_impl->pipeline) {
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
        m_impl->pipeline = context.device().createGraphicsPipeline(pipeline_desc,
                m_impl->current_framebuffer->getFramebufferInfo());
        if (!m_impl->pipeline) {
            throw std::runtime_error("NVRHI failed to create the instancing pipeline.");
        }
    }
}

void InstancingPass::record(renderer::RenderPassContext& context) {
    if (!m_impl->pipeline || !m_impl->current_depth_buffer || !m_impl->current_framebuffer ||
            !m_impl->binding_set) {
        throw std::logic_error("InstancingPass was not prepared.");
    }

    auto& commands = context.commands();
    commands.clearTextureFloat(&context.colorTexture(), nvrhi::AllSubresources,
            nvrhi::Color{ 0.018f, 0.024f, 0.035f, 1.0f });
    commands.clearDepthStencilTexture(m_impl->current_depth_buffer, nvrhi::AllSubresources, true,
            1.0f, false, 0);

    const auto& framebuffer_info = m_impl->current_framebuffer->getFramebufferInfo();
    nvrhi::ViewportState viewport;
    viewport.addViewportAndScissorRect(framebuffer_info.getViewport());
    nvrhi::GraphicsState state;
    state.setPipeline(m_impl->pipeline)
            .setFramebuffer(m_impl->current_framebuffer)
            .setViewport(viewport)
            .addBindingSet(m_impl->binding_set)
            .addVertexBuffer(nvrhi::VertexBufferBinding()
                            .setBuffer(&context.buffer(m_impl->vertex_buffer))
                            .setSlot(0))
            .addVertexBuffer(nvrhi::VertexBufferBinding()
                            .setBuffer(&context.buffer(m_impl->instance_buffer))
                            .setSlot(1))
            .setIndexBuffer(nvrhi::IndexBufferBinding()
                            .setBuffer(&context.buffer(m_impl->index_buffer))
                            .setFormat(nvrhi::Format::R32_UINT));
    commands.setGraphicsState(state);

    const float aspect = static_cast<float>(framebuffer_info.width) /
                         static_cast<float>(framebuffer_info.height);
    const float vertical_half_fov = glm::radians(kVerticalFieldOfViewDegrees * 0.5f);
    const float horizontal_half_fov = std::atan(std::tan(vertical_half_fov) * aspect);
    const float limiting_half_fov = std::min(vertical_half_fov, horizontal_half_fov);
    const float grid_half_extent =
            kGridCenter * kGridSpacing + kCubeExtent * kMaximumCubeScale + kMaximumBobOffset;
    const float grid_bounding_radius = std::sqrt(3.0f) * grid_half_extent;
    const float camera_distance =
            grid_bounding_radius / std::sin(limiting_half_fov) * kCameraFitMargin;
    const float camera_angle = 0.72f + m_impl->time * 0.12f;
    constexpr float camera_elevation = glm::radians(28.0f);
    const float horizontal_camera_distance = std::cos(camera_elevation) * camera_distance;
    const glm::vec3 camera_position{ std::cos(camera_angle) * horizontal_camera_distance,
        std::sin(camera_elevation) * camera_distance,
        std::sin(camera_angle) * horizontal_camera_distance };
    const glm::mat4 view =
            glm::lookAt(camera_position, glm::vec3{ 0.0f }, glm::vec3{ 0.0f, 1.0f, 0.0f });
    const float clip_margin = grid_bounding_radius * 1.25f;
    const float near_plane = std::max(0.1f, camera_distance - clip_margin);
    const float far_plane = camera_distance + clip_margin;
    const glm::mat4 projection = glm::perspective(glm::radians(kVerticalFieldOfViewDegrees), aspect,
            near_plane, far_plane);
    const glm::mat4 view_projection = projection * view;
    InstancingPushConstants push_constants{};
    std::memcpy(push_constants.view_projection.data(), glm::value_ptr(view_projection),
            sizeof(view_projection));
    push_constants.animation[0] = m_impl->time;
    push_constants.animation[1] = camera_position.x;
    push_constants.animation[2] = camera_position.y;
    push_constants.animation[3] = camera_position.z;
    commands.setPushConstants(&push_constants, sizeof(push_constants));
    commands.drawIndexed(nvrhi::DrawArguments{}
                    .setVertexCount(m_impl->index_buffer.indexCount())
                    .setInstanceCount(static_cast<uint32_t>(kInstanceCount)));
}

} // namespace arti::instancing
