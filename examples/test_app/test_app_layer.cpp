#include "test_app_layer.h"
#include "application.h"
#include "artichoco/core/task/task_system.h"
#include "artichoco/platform/window/sdl_vulkan_surface_source.h"
#include "artichoco/project/project_manager.h"
#include "artichoco/renderer/render_device.h"
#include "artichoco/renderer/slang_compiler.h"
#include "artichoco/renderer/vulkan/nvrhi_shader_factory.h"
#include "artichoco/scene/scene.h"
#include "artichoco/scene/scene_serialization_registry.h"
#include "artichoco/scene/scene_serializer.h"
#include "camera_controller_system.h"
#include "cubemap_attachment_pass.h"
#include "image_loader.h"
#include "render_system.h"
#include "rotation_system.h"
#include "scene_component_serialization.h"
#include "texture_compute_pass.h"
#include "throw_once_pass.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cstddef>

#include <array>
#include <atomic>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace arti::test_app {
namespace {

struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;
};

static_assert(std::is_standard_layout_v<Vertex>);

constexpr std::array vertices = {
    Vertex{ { -0.8f, -0.8f, 0.8f }, { 0.0f, 1.0f } },
    Vertex{ { 0.8f, -0.8f, 0.8f }, { 1.0f, 1.0f } },
    Vertex{ { 0.8f, 0.8f, 0.8f }, { 1.0f, 0.0f } },
    Vertex{ { -0.8f, 0.8f, 0.8f }, { 0.0f, 0.0f } },

    Vertex{ { 0.8f, -0.8f, -0.8f }, { 0.0f, 1.0f } },
    Vertex{ { -0.8f, -0.8f, -0.8f }, { 1.0f, 1.0f } },
    Vertex{ { -0.8f, 0.8f, -0.8f }, { 1.0f, 0.0f } },
    Vertex{ { 0.8f, 0.8f, -0.8f }, { 0.0f, 0.0f } },

    Vertex{ { 0.8f, -0.8f, 0.8f }, { 0.0f, 1.0f } },
    Vertex{ { 0.8f, -0.8f, -0.8f }, { 1.0f, 1.0f } },
    Vertex{ { 0.8f, 0.8f, -0.8f }, { 1.0f, 0.0f } },
    Vertex{ { 0.8f, 0.8f, 0.8f }, { 0.0f, 0.0f } },

    Vertex{ { -0.8f, -0.8f, -0.8f }, { 0.0f, 1.0f } },
    Vertex{ { -0.8f, -0.8f, 0.8f }, { 1.0f, 1.0f } },
    Vertex{ { -0.8f, 0.8f, 0.8f }, { 1.0f, 0.0f } },
    Vertex{ { -0.8f, 0.8f, -0.8f }, { 0.0f, 0.0f } },

    Vertex{ { -0.8f, 0.8f, 0.8f }, { 0.0f, 1.0f } },
    Vertex{ { 0.8f, 0.8f, 0.8f }, { 1.0f, 1.0f } },
    Vertex{ { 0.8f, 0.8f, -0.8f }, { 1.0f, 0.0f } },
    Vertex{ { -0.8f, 0.8f, -0.8f }, { 0.0f, 0.0f } },

    Vertex{ { -0.8f, -0.8f, -0.8f }, { 0.0f, 1.0f } },
    Vertex{ { 0.8f, -0.8f, -0.8f }, { 1.0f, 1.0f } },
    Vertex{ { 0.8f, -0.8f, 0.8f }, { 1.0f, 0.0f } },
    Vertex{ { -0.8f, -0.8f, 0.8f }, { 0.0f, 0.0f } },
};

constexpr std::array<uint32_t, 36> indices = {
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
    layout.stride = sizeof(Vertex);
    layout.attributes = {
        { 0, renderer::VertexAttributeType::Float3, offsetof(Vertex, position) },
        { 1, renderer::VertexAttributeType::Float2, offsetof(Vertex, uv) },
    };
    return layout;
}

class SystemToggleProbe final : public scene::SceneSystem {
public:
    void onUpdate(scene::Scene& scene, const scene::UpdateContext& context) override {
        if (context.frameIndex == 2) {
            scene.setSystemEnabled<RotationSystem>(false);
        }
    }
};

struct NvrhiTriangleVertex {
    float position[3];
    float color[3];
};

class NvrhiTrianglePass final : public renderer::RenderPass {
public:
    NvrhiTrianglePass(renderer::VertexBuffer vertex_buffer, std::filesystem::path shader_path)
            : m_vertex_buffer(std::move(vertex_buffer)),
              m_shader_path(std::move(shader_path)) {}

    void record(renderer::RenderPassContext& context) override {
        if (!m_pipeline) {
            throw std::logic_error("NVRHI triangle pass was not prepared.");
        }
        context.commands().clearTextureFloat(&context.colorTexture(), nvrhi::AllSubresources,
                nvrhi::Color{ 0.03f, 0.07f, 0.12f, 1.0f });

        nvrhi::GraphicsState state;
        nvrhi::ViewportState viewport;
        viewport.addViewportAndScissorRect(context.framebufferInfo().getViewport());
        state.setPipeline(m_pipeline)
                .setFramebuffer(&context.framebuffer())
                .setViewport(viewport)
                .addVertexBuffer(nvrhi::VertexBufferBinding()
                                .setBuffer(&context.buffer(m_vertex_buffer))
                                .setSlot(0));
        context.commands().setGraphicsState(state);
        context.commands().draw(nvrhi::DrawArguments{}.setVertexCount(3));
    }

    void prepare(renderer::RenderPassPrepareContext& context) override {
        if (m_pipeline) {
            return;
        }
        const renderer::CompiledGraphicsProgram program =
                renderer::SlangCompiler::compileGraphics({ m_shader_path });
        const auto shaders = renderer::vulkan::createNvrhiGraphicsShaderSet(context.device(),
                program, "ArtiChoco NVRHI triangle smoke");

        nvrhi::VertexAttributeDesc position;
        position.setName("POSITION")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(0)
                .setElementStride(sizeof(NvrhiTriangleVertex));
        nvrhi::VertexAttributeDesc color;
        color.setName("COLOR0")
                .setFormat(nvrhi::Format::RGB32_FLOAT)
                .setBufferIndex(0)
                .setOffset(sizeof(float) * 3)
                .setElementStride(sizeof(NvrhiTriangleVertex));
        const std::array attributes = { position, color };
        nvrhi::InputLayoutHandle input_layout = context.device().createInputLayout(
                attributes.data(), attributes.size(), shaders.vertex_shader);
        if (!input_layout) {
            throw std::runtime_error("NVRHI failed to create the triangle input layout.");
        }

        nvrhi::GraphicsPipelineDesc pipeline_desc;
        nvrhi::DepthStencilState depth_state;
        depth_state.disableDepthTest().disableDepthWrite().disableStencil();
        nvrhi::RasterState raster_state;
        raster_state.setCullBack().setFrontCounterClockwise(true);
        nvrhi::RenderState render_state;
        render_state.setDepthStencilState(depth_state).setRasterState(raster_state);
        pipeline_desc.setPrimType(nvrhi::PrimitiveType::TriangleList)
                .setInputLayout(input_layout)
                .setVertexShader(shaders.vertex_shader)
                .setPixelShader(shaders.pixel_shader)
                .setRenderState(render_state);
        for (const nvrhi::BindingLayoutHandle& layout: shaders.binding_layouts) {
            if (layout) {
                pipeline_desc.addBindingLayout(layout);
            }
        }
        m_pipeline = context.device().createGraphicsPipeline(pipeline_desc,
                context.framebuffer().getFramebufferInfo());
        if (!m_pipeline) {
            throw std::runtime_error("NVRHI failed to create the triangle graphics pipeline.");
        }
    }

private:
    renderer::VertexBuffer m_vertex_buffer;
    std::filesystem::path m_shader_path;
    nvrhi::GraphicsPipelineHandle m_pipeline;
};

} // namespace

TestAppLayer::TestAppLayer(bool enable_renderer, bool smoke_render, bool smoke_nvrhi)
        : Layer("TestAppLayer"),
          m_enable_renderer(enable_renderer),
          m_smoke_render(smoke_render),
          m_smoke_nvrhi(smoke_nvrhi) {}

TestAppLayer::~TestAppLayer() = default;

void TestAppLayer::onAttach() {
    core::Application::get().getLogChannel().info("hello world");

    if (m_smoke_nvrhi) {
        auto surface_source =
                platform::createSDLVulkanSurfaceSource(core::Application::get().getWindow());
        renderer::RenderDeviceCreateInfo render_device_info;
        render_device_info.application_name = "NVRHI Smoke";
        m_render_device =
                std::make_unique<renderer::RenderDevice>(core::Application::get().getWindow(),
                        std::move(surface_source), render_device_info);
        constexpr std::array triangle_vertices = {
            NvrhiTriangleVertex{ { 0.0f, 0.75f, 0.0f }, { 1.0f, 0.1f, 0.1f } },
            NvrhiTriangleVertex{ { -0.75f, -0.75f, 0.0f }, { 0.1f, 0.2f, 1.0f } },
            NvrhiTriangleVertex{ { 0.75f, -0.75f, 0.0f }, { 0.1f, 1.0f, 0.1f } },
        };
        renderer::VertexBufferLayout triangle_layout;
        triangle_layout.stride = sizeof(NvrhiTriangleVertex);
        triangle_layout.attributes = {
            { 0, renderer::VertexAttributeType::Float3, 0 },
            { 1, renderer::VertexAttributeType::Float3, sizeof(float) * 3 },
        };
        m_nvrhi_clear_pass = std::make_unique<NvrhiTrianglePass>(
                m_render_device->createVertexBuffer(std::as_bytes(std::span{ triangle_vertices }),
                        static_cast<uint32_t>(triangle_vertices.size()), triangle_layout),
                ARTI_TEST_TRIANGLE_SHADER_PATH);
        constexpr std::array<uint8_t, 4 * 4 * 4> compute_source_pixels = {
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
            220,
            48,
            20,
            255,
        };
        auto compute_source = std::make_shared<renderer::Texture2D>(
                m_render_device->createTexture2D(std::as_bytes(std::span{ compute_source_pixels }),
                        4, 4, renderer::TextureFormat::RGBA8Unorm, false));
        m_nvrhi_texture_compute_pass = std::make_unique<TextureComputePass>(
                std::move(compute_source), ARTI_TEST_COMPUTE_SHADER_PATH);
        m_nvrhi_cubemap_attachment_pass =
                std::make_unique<CubemapAttachmentPass>(ARTI_TEST_CUBEMAP_SHADER_PATH);
        if (!m_render_device->nvrhiResourceSmoke()) {
            throw std::runtime_error("NVRHI resource creation/upload smoke test failed.");
        }
        if (!m_render_device->nvrhiComputeShaderSmoke(ARTI_TEST_COMPUTE_SHADER_PATH)) {
            throw std::runtime_error("NVRHI Slang compute shader smoke test failed.");
        }
        m_render_frames_remaining = 3;
        core::Application::get().getLogChannel().info(
                "NVRHI Vulkan device and Slang reflection smoke initialized");
        return;
    }

    if (m_enable_renderer) {
        auto surface_source =
                platform::createSDLVulkanSurfaceSource(core::Application::get().getWindow());
        renderer::RenderDeviceCreateInfo render_device_info;
        render_device_info.application_name = "Test App";
        m_render_device =
                std::make_unique<renderer::RenderDevice>(core::Application::get().getWindow(),
                        std::move(surface_source), render_device_info);

        m_scene = std::make_unique<scene::Scene>();
        scene::Scene::registerComponentCopy<MeshComponent>();
        scene::Scene::registerComponentCopy<MaterialComponent>();
        scene::Scene::registerComponentCopy<RotationComponent>();
        m_scene->addSystem<CameraControllerSystem>(scene::SystemStage::Update);
        m_scene->addSystem<RotationSystem>(scene::SystemStage::Update);
        m_scene->addSystem<SystemToggleProbe>(scene::SystemStage::Update);
        m_scene->addSystem<RenderSystem>(scene::SystemStage::RenderExtract, *m_render_device,
                core::Application::get().getWindow(), size_t{ 3 }, ARTI_TEST_COMPUTE_SHADER_PATH,
                ARTI_TEST_MESH_SHADER_PATH, ARTI_TEST_COMPOSITE_SHADER_PATH);

        auto vertex_buffer =
                m_render_device->createVertexBuffer(std::as_bytes(std::span{ vertices }),
                        static_cast<uint32_t>(vertices.size()), cubeVertexLayout());
        auto index_buffer = m_render_device->createIndexBuffer(std::as_bytes(std::span{ indices }),
                static_cast<uint32_t>(indices.size()));

        const ImageData brownie = loadImageRGBA(ARTI_TEST_BROWNIE_TEXTURE_PATH);
        auto texture = m_render_device->createTexture2D(brownie.rgba_pixels, brownie.width,
                brownie.height, renderer::TextureFormat::RGBA8Srgb);

        auto cube = m_scene->createEntity("cube");
        cube.addComponent<MeshComponent>(Mesh{ std::move(vertex_buffer), std::move(index_buffer) });
        cube.addComponent<MaterialComponent>(Material{ std::move(texture) });
        cube.addComponent<RotationComponent>(glm::vec3{ 0.7f, 1.0f, 0.35f }, 0.85f);
        m_cube_entity = cube;

        auto cube_secondary = m_scene->createEntity("cube_secondary");
        cube_secondary.addComponent<MeshComponent>(cube.getComponent<MeshComponent>().mesh);
        cube_secondary.addComponent<MaterialComponent>(
                cube.getComponent<MaterialComponent>().material);
        cube_secondary.addComponent<RotationComponent>(glm::vec3{ 0.0f, 1.0f, 0.0f }, -1.3f);
        auto& secondary_transform = cube_secondary.getComponent<scene::TransformComponent>();
        secondary_transform.translation = glm::vec3{ 2.0f, 0.0f, 0.0f };
        m_cube_secondary_entity = cube_secondary;

        auto camera = m_scene->createEntity("camera");
        auto& camera_transform = camera.getComponent<scene::TransformComponent>();
        camera_transform.translation = glm::vec3{ 2.8f, 2.2f, 3.2f };
        camera_transform.rotation = glm::quatLookAt(glm::normalize(-camera_transform.translation),
                glm::vec3{ 0.0f, 1.0f, 0.0f });
        camera.addComponent<CameraComponent>();

        auto hierarchy_parent = m_scene->createEntity("hierarchy_parent");
        auto& hierarchy_parent_transform =
                hierarchy_parent.getComponent<scene::TransformComponent>();
        hierarchy_parent_transform.translation = glm::vec3{ 1.0f, 2.0f, 3.0f };
        hierarchy_parent_transform.rotation =
                glm::angleAxis(glm::radians(90.0f), glm::vec3{ 0.0f, 1.0f, 0.0f });
        m_hierarchy_parent_entity = hierarchy_parent;

        auto hierarchy_child = m_scene->createEntity("hierarchy_child");
        m_scene->setParent(hierarchy_child, hierarchy_parent);
        auto& hierarchy_child_transform = hierarchy_child.getComponent<scene::TransformComponent>();
        hierarchy_child_transform.translation = glm::vec3{ 4.0f, 5.0f, 6.0f };
        m_hierarchy_child_entity = hierarchy_child;

        if (m_smoke_render) {
            m_throw_once_pass = std::make_unique<ThrowOncePass>();
        }
        m_render_frames_remaining = m_smoke_render ? 5 : 0;
        core::Application::get().getLogChannel().info(
                "Rendering MRT cube demo with brownie.png ({}x{})", brownie.width, brownie.height);
        return;
    }
}

void TestAppLayer::onDetach() {
    m_scene.reset();
    if (m_render_device) {
        m_render_device->waitIdle();
    }
    m_nvrhi_cubemap_attachment_pass.reset();
    m_nvrhi_texture_compute_pass.reset();
    m_nvrhi_clear_pass.reset();
    m_render_device.reset();
}

void TestAppLayer::onUpdate(core::Timestep delta_time) {
    m_delta_time = delta_time;
    if (!m_scene) {
        return;
    }

    m_fixed_timestep.tick(delta_time.getSeconds(), [this](float fixed_dt) {
        scene::UpdateContext fixed_context;
        fixed_context.deltaTime = core::Timestep{ fixed_dt };
        fixed_context.fixedDeltaTime = core::Timestep{ fixed_dt };
        fixed_context.frameIndex = m_fixed_frame_index++;
        m_scene->runSystems(scene::SystemStage::FixedUpdate, fixed_context);
    });

    scene::UpdateContext context;
    context.deltaTime = delta_time;
    context.fixedDeltaTime = core::Timestep{ m_fixed_timestep.fixedDeltaTime() };
    context.frameIndex = m_frame_index;
    m_scene->runSystems(scene::SystemStage::Update, context);
    m_scene->runSystems(scene::SystemStage::LateUpdate, context);

    if (m_smoke_render && m_frame_index == 1) {
        const auto& render_system = m_scene->getSystem<RenderSystem>();
        if (render_system.lastDrawCount() != 2) {
            throw std::runtime_error("The render frame data does not contain both cube draws.");
        }
    }

    if (m_smoke_render && m_frame_index == 2) {
        m_rotation_snapshot = m_cube_entity.getComponent<scene::TransformComponent>().rotation;
    } else if (m_smoke_render && m_frame_index == 3) {
        const auto& rotation = m_cube_entity.getComponent<scene::TransformComponent>().rotation;
        if (glm::length(rotation - m_rotation_snapshot) > 1e-6f) {
            throw std::runtime_error("A disabled System still ran.");
        }
        m_scene->setSystemEnabled<RotationSystem>(true);
    } else if (m_smoke_render && m_frame_index == 4) {
        const auto& rotation = m_cube_entity.getComponent<scene::TransformComponent>().rotation;
        if (glm::length(rotation - m_rotation_snapshot) <= 1e-6f) {
            throw std::runtime_error("A re-enabled System did not run.");
        }
    }

    if (m_smoke_render && m_frame_index == 0) {
        verifyHierarchy();
        verifySnapshot();
        verifyTaskSystem();
        verifyMultiEntity();
        verifyProject();
    }
}

void TestAppLayer::verifySerialization() {
    scene::SceneSerializationRegistry registry;
    registry.registerComponent<RotationComponent>("test.rotation",
            std::make_unique<RotationComponentSerialization>());
    scene::SceneSerializer serializer{ registry };

    scene::Scene source;
    auto parent = source.createEntity("serialized_parent");
    auto& parent_transform = parent.getComponent<scene::TransformComponent>();
    parent_transform.translation = glm::vec3{ 1.0f, 2.0f, 3.0f };
    parent.addComponent<RotationComponent>(glm::vec3{ 0.0f, 1.0f, 0.0f }, 2.5f);

    auto child = source.createEntity("serialized_child");
    child.getComponent<scene::TransformComponent>().translation = glm::vec3{ 4.0f, 5.0f, 6.0f };
    source.setParent(child, parent);

    const std::string yaml = YAML::Dump(serializer.serialize(source));
    scene::Scene restored;
    serializer.deserialize(YAML::Load(yaml), restored);

    const auto restored_parent = restored.findEntity(parent.getUUID());
    const auto restored_child = restored.findEntity(child.getUUID());
    if (!restored_parent || !restored_child ||
            restored_parent.getComponent<scene::TagComponent>().tag != "serialized_parent" ||
            restored.getParent(restored_child) != restored_parent) {
        throw std::runtime_error(
                "Scene serialization did not restore Entity metadata or hierarchy.");
    }

    const auto& restored_rotation = restored_parent.getComponent<RotationComponent>();
    if (glm::length(restored_rotation.axis - glm::vec3{ 0.0f, 1.0f, 0.0f }) > 1e-5f ||
            std::abs(restored_rotation.speed - 2.5f) > 1e-5f) {
        throw std::runtime_error(
                "Scene serialization did not restore a registered developer Component.");
    }

    const glm::mat4 expected_world =
            restored_parent.getComponent<scene::TransformComponent>().getTransform() *
            restored_child.getComponent<scene::TransformComponent>().getTransform();
    const glm::mat4& restored_world = restored.getWorldTransform(restored_child);
    for (size_t index = 0; index < 16; ++index) {
        if (std::abs(glm::value_ptr(restored_world)[index] -
                     glm::value_ptr(expected_world)[index]) > 1e-5f) {
            throw std::runtime_error("Scene serialization did not rebuild world transforms.");
        }
    }
}

void TestAppLayer::verifySnapshot() {
    const auto& cube_mesh = m_cube_entity.getComponent<MeshComponent>().mesh;
    const auto& cube_material = m_cube_entity.getComponent<MaterialComponent>().material;

    scene::Scene snapshot_scene;
    auto snap_mesh = snapshot_scene.createEntity("snap_mesh");
    auto& snap_transform = snap_mesh.getComponent<scene::TransformComponent>();
    snap_transform.translation = glm::vec3{ 10.0f, 20.0f, 30.0f };
    snap_mesh.addComponent<MeshComponent>(cube_mesh);
    snap_mesh.addComponent<MaterialComponent>(cube_material);
    snap_mesh.addComponent<RotationComponent>(glm::vec3{ 0.0f, 1.0f, 0.0f }, 2.0f);
    auto snap_child = snapshot_scene.createEntity("snap_child");
    snapshot_scene.setParent(snap_child, snap_mesh);

    scene::Scene snapshot;
    snapshot.copyEntitiesFrom(snapshot_scene);

    snapshot_scene.destroyEntity(snap_mesh);

    snapshot_scene.copyEntitiesFrom(snapshot);

    const auto restored = snapshot_scene.findEntityByTag("snap_mesh");
    if (!restored) {
        throw std::runtime_error("Scene snapshot restore lost the mesh entity.");
    }
    const auto& restored_transform = restored.getComponent<scene::TransformComponent>();
    if (glm::length(restored_transform.translation - glm::vec3{ 10.0f, 20.0f, 30.0f }) > 1e-5f) {
        throw std::runtime_error("Scene snapshot restore changed the transform.");
    }
    const auto& restored_rotation = restored.getComponent<RotationComponent>();
    if (glm::length(restored_rotation.axis - glm::vec3{ 0.0f, 1.0f, 0.0f }) > 1e-5f ||
            std::abs(restored_rotation.speed - 2.0f) > 1e-5f) {
        throw std::runtime_error("Scene snapshot restore changed a registered component.");
    }
    const auto& restored_mesh = restored.getComponent<MeshComponent>().mesh;
    const auto& restored_material = restored.getComponent<MaterialComponent>().material;
    if (!restored_mesh.sharesBuffersWith(cube_mesh) ||
            !restored_material.sharesTextureWith(cube_material)) {
        throw std::runtime_error("Scene snapshot restore did not share GPU resources.");
    }

    const auto restored_child = snapshot_scene.findEntityByTag("snap_child");
    if (!restored_child) {
        throw std::runtime_error("Scene snapshot restore lost the child entity.");
    }
    if (snapshot_scene.getParent(restored_child) != restored) {
        throw std::runtime_error("Scene snapshot restore broke the hierarchy.");
    }
}

void TestAppLayer::verifyMultiEntity() {
    const auto secondary = m_scene->findEntityByTag("cube_secondary");
    if (!secondary) {
        throw std::runtime_error("The secondary cube entity is missing.");
    }
    const auto& secondary_transform = secondary.getComponent<scene::TransformComponent>();
    if (glm::length(secondary_transform.translation - glm::vec3{ 2.0f, 0.0f, 0.0f }) > 1e-5f) {
        throw std::runtime_error("The secondary cube transform is incorrect.");
    }
    const auto& primary_mesh = m_cube_entity.getComponent<MeshComponent>().mesh;
    const auto& secondary_mesh = secondary.getComponent<MeshComponent>().mesh;
    if (!secondary_mesh.sharesBuffersWith(primary_mesh)) {
        throw std::runtime_error(
                "The secondary cube does not share GPU resources with the primary cube.");
    }
}

void TestAppLayer::verifyProject() {
    namespace fs = std::filesystem;

    const fs::path temp_root = fs::temp_directory_path() / "arti_test_project";
    fs::remove_all(temp_root);
    std::error_code error;

    auto& manager = project::ProjectManager::instance();
    project::ProjectInfo info;
    info.name = "verify_project";
    info.version = "1.2.3";
    info.author = "smoke";
    info.assets_path = "assets";
    if (!manager.createProject(temp_root, info) || !manager.saveProject()) {
        throw std::runtime_error("ProjectManager create/save failed.");
    }
    if (!manager.loadProject(temp_root / "verify_project.artiproj")) {
        throw std::runtime_error("ProjectManager load failed.");
    }

    const auto& loaded = manager.getProjectInfo();
    if (!loaded) {
        throw std::runtime_error("ProjectManager loaded no project info.");
    }
    if (loaded->name != "verify_project" || loaded->version != "1.2.3" ||
            loaded->author != "smoke" || loaded->assets_path.generic_string() != "assets") {
        throw std::runtime_error("ProjectManager roundtrip changed project fields.");
    }
    if (!manager.getProjectRootPath() ||
            manager.getProjectRootPath().value() != fs::absolute(temp_root)) {
        throw std::runtime_error("ProjectManager reported an incorrect project root.");
    }

    fs::remove_all(temp_root, error);
}

void TestAppLayer::verifyTaskSystem() {
    constexpr uint32_t count = 4096;
    std::vector<uint32_t> results(count, 0);
    core::TaskSystem::get().parallelFor(count,
            [&results](uint32_t index) { results[index] = index * 2; });
    for (uint32_t index = 0; index < count; ++index) {
        if (results[index] != index * 2) {
            throw std::runtime_error("TaskSystem parallelFor produced incorrect results.");
        }
    }

    std::atomic<uint32_t> submitted_count{ 0 };
    for (uint32_t index = 0; index < 32; ++index) {
        core::TaskSystem::get().submit([&submitted_count] { submitted_count.fetch_add(1); });
    }
    core::TaskSystem::get().waitForAll();
    if (submitted_count.load() != 32) {
        throw std::runtime_error("TaskSystem submit did not complete all tasks.");
    }
}

void TestAppLayer::verifyHierarchy() {
    const auto& child_world = m_scene->getWorldTransform(m_hierarchy_child_entity);
    const auto& parent_transform =
            m_hierarchy_parent_entity.getComponent<scene::TransformComponent>();
    const auto& child_transform =
            m_hierarchy_child_entity.getComponent<scene::TransformComponent>();
    const glm::mat4 expected = parent_transform.getTransform() * child_transform.getTransform();
    for (size_t index = 0; index < 16; ++index) {
        if (std::abs(glm::value_ptr(child_world)[index] - glm::value_ptr(expected)[index]) >
                1e-5f) {
            throw std::runtime_error("Hierarchy world transform propagation failed.");
        }
    }

    const std::vector<scene::Entity> children = m_scene->getChildren(m_hierarchy_parent_entity);
    if (children.size() != 1 || children.front() != m_hierarchy_child_entity) {
        throw std::runtime_error("Hierarchy child enumeration failed.");
    }

    if (m_scene->findEntityByTag("hierarchy_child") != m_hierarchy_child_entity) {
        throw std::runtime_error("findEntityByTag lookup failed.");
    }
    if (m_scene->findEntityByTag("no_such_tag")) {
        throw std::runtime_error("findEntityByTag returned a match for an unknown tag.");
    }

    m_scene->destroyEntity(m_hierarchy_parent_entity);
    if (m_scene->isValid(m_hierarchy_child_entity)) {
        throw std::runtime_error("Hierarchy cascade destruction failed.");
    }
    m_hierarchy_parent_entity = {};
    m_hierarchy_child_entity = {};
}

void TestAppLayer::onRender() {
    if (m_smoke_nvrhi) {
        renderer::RenderPass* clear_pass = m_nvrhi_clear_pass.get();
        renderer::RenderPass* compute_pass = m_nvrhi_texture_compute_pass.get();
        renderer::RenderPass* cubemap_pass = m_nvrhi_cubemap_attachment_pass.get();
        const std::array passes = { compute_pass, cubemap_pass, clear_pass };
        if (!m_render_device->renderFrame(
                    std::span<renderer::RenderPass* const>{ passes.data(), passes.size() })) {
            return;
        }
        if (!m_nvrhi_cubemap_verified) {
            m_render_device->waitIdle();
            if (!m_nvrhi_cubemap_attachment_pass->verifyReadback()) {
                const auto pixel = m_nvrhi_cubemap_attachment_pass->readbackPixel();
                throw std::runtime_error(
                        "NVRHI cubemap attachment readback verification failed: actual RGBA (" +
                        std::to_string(pixel[0]) + ", " + std::to_string(pixel[1]) + ", " +
                        std::to_string(pixel[2]) + ", " + std::to_string(pixel[3]) + ").");
            }
            m_nvrhi_cubemap_verified = true;
            core::Application::get().getLogChannel().info(
                    "NVRHI cubemap attachment smoke test passed");
        }
        if (m_render_frames_remaining == 3) {
            core::Application::get().getWindow().resize(960, 540);
        }
        if (--m_render_frames_remaining == 0) {
            core::Application::get().getLogChannel().info(
                    "NVRHI Vulkan RenderPass clear/present/resize smoke test passed");
            core::Application::get().close();
        }
        return;
    }

    if (!m_scene) {
        return;
    }

    scene::UpdateContext context;
    context.deltaTime = m_delta_time;
    context.fixedDeltaTime = core::Timestep{ m_fixed_timestep.fixedDeltaTime() };
    context.frameIndex = m_frame_index;

    auto& render_system = m_scene->getSystem<RenderSystem>();

    if (m_throw_once_pass) {
        renderer::RenderPass* throw_pass = m_throw_once_pass.get();
        render_system.prependPass(throw_pass);
        m_scene->runSystems(scene::SystemStage::RenderExtract, context);
        render_system.waitForFrameComplete();
        render_system.removePass(throw_pass);
        m_throw_once_pass.reset();

        std::exception_ptr error = render_system.consumeRenderError();
        if (!error) {
            throw std::logic_error("The Vulkan frame recovery test pass did not throw.");
        }
        bool expected_failure = false;
        try {
            std::rethrow_exception(error);
        } catch (const std::runtime_error& exception) {
            expected_failure = std::string_view{ exception.what() } ==
                               "Intentional NVRHI frame recording failure.";
        } catch (...) {
        }
        if (!expected_failure) {
            std::rethrow_exception(error);
        }

        m_scene->runSystems(scene::SystemStage::RenderExtract, context);
        render_system.waitForFrameComplete();
        if (render_system.consumeRenderError()) {
            throw std::runtime_error("The NVRHI frame recording recovery did not succeed.");
        }
        core::Application::get().getLogChannel().info(
                "NVRHI frame recording recovery smoke test passed");
    } else {
        m_scene->runSystems(scene::SystemStage::RenderExtract, context);
        if (m_smoke_render) {
            render_system.waitForFrameComplete();
        }
    }

    m_frame_index++;
    if (!m_smoke_render) {
        return;
    }
    if (m_render_frames_remaining == 5) {
        auto replacement_vertices =
                m_render_device->createVertexBuffer(std::as_bytes(std::span{ vertices }),
                        static_cast<uint32_t>(vertices.size()), cubeVertexLayout());
        auto replacement_indices = m_render_device->createIndexBuffer(
                std::as_bytes(std::span{ indices }), static_cast<uint32_t>(indices.size()));
        m_cube_entity.getComponent<MeshComponent>() = MeshComponent{ Mesh{
            std::move(replacement_vertices), std::move(replacement_indices) } };

        const ImageData brownie = loadImageRGBA(ARTI_TEST_BROWNIE_TEXTURE_PATH);
        auto replacement_texture = m_render_device->createTexture2D(brownie.rgba_pixels,
                brownie.width, brownie.height, renderer::TextureFormat::RGBA8Srgb);
        m_cube_entity.getComponent<MaterialComponent>() =
                MaterialComponent{ Material{ std::move(replacement_texture) } };
        core::Application::get().getWindow().resize(960, 540);
    }
    if (--m_render_frames_remaining == 0) {
        render_system.waitForFrameComplete();
        core::Application::get().getLogChannel().info("NVRHI MRT cube smoke test passed");
        core::Application::get().close();
    }
}

} // namespace arti::test_app
