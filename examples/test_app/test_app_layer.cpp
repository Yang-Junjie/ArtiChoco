#include "application.h"
#include "artichoco/platform/window/sdl_vulkan_surface_source.h"
#include "artichoco/renderer/render_device.h"
#include "artichoco/renderer/vulkan/vulkan_allocator.h"
#include "artichoco/renderer/vulkan/vulkan_context.h"
#include "artichoco/renderer/vulkan/vulkan_device.h"
#include "artichoco/renderer/vulkan/vulkan_surface.h"
#include "artichoco/scene/scene.h"
#include "camera_controller_system.h"
#include "image_loader.h"
#include "render_system.h"
#include "rotation_system.h"
#include "test_app_layer.h"
#include "throw_once_pass.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cstddef>
#include <cstring>

#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace arti::test_app {
namespace {

struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;
};

static_assert(std::is_standard_layout_v<Vertex>);

constexpr std::array vertices = {
    Vertex{{-0.8f, -0.8f, 0.8f}, {0.0f, 1.0f}},  Vertex{{0.8f, -0.8f, 0.8f}, {1.0f, 1.0f}},
    Vertex{{0.8f, 0.8f, 0.8f}, {1.0f, 0.0f}},    Vertex{{-0.8f, 0.8f, 0.8f}, {0.0f, 0.0f}},

    Vertex{{0.8f, -0.8f, -0.8f}, {0.0f, 1.0f}},  Vertex{{-0.8f, -0.8f, -0.8f}, {1.0f, 1.0f}},
    Vertex{{-0.8f, 0.8f, -0.8f}, {1.0f, 0.0f}},  Vertex{{0.8f, 0.8f, -0.8f}, {0.0f, 0.0f}},

    Vertex{{0.8f, -0.8f, 0.8f}, {0.0f, 1.0f}},   Vertex{{0.8f, -0.8f, -0.8f}, {1.0f, 1.0f}},
    Vertex{{0.8f, 0.8f, -0.8f}, {1.0f, 0.0f}},   Vertex{{0.8f, 0.8f, 0.8f}, {0.0f, 0.0f}},

    Vertex{{-0.8f, -0.8f, -0.8f}, {0.0f, 1.0f}}, Vertex{{-0.8f, -0.8f, 0.8f}, {1.0f, 1.0f}},
    Vertex{{-0.8f, 0.8f, 0.8f}, {1.0f, 0.0f}},   Vertex{{-0.8f, 0.8f, -0.8f}, {0.0f, 0.0f}},

    Vertex{{-0.8f, 0.8f, 0.8f}, {0.0f, 1.0f}},   Vertex{{0.8f, 0.8f, 0.8f}, {1.0f, 1.0f}},
    Vertex{{0.8f, 0.8f, -0.8f}, {1.0f, 0.0f}},   Vertex{{-0.8f, 0.8f, -0.8f}, {0.0f, 0.0f}},

    Vertex{{-0.8f, -0.8f, -0.8f}, {0.0f, 1.0f}}, Vertex{{0.8f, -0.8f, -0.8f}, {1.0f, 1.0f}},
    Vertex{{0.8f, -0.8f, 0.8f}, {1.0f, 0.0f}},   Vertex{{-0.8f, -0.8f, 0.8f}, {0.0f, 0.0f}},
};

constexpr std::array<uint32_t, 36> indices = {
    0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
    12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20,
};

renderer::VertexBufferLayout cubeVertexLayout()
{
    renderer::VertexBufferLayout layout;
    layout.stride = sizeof(Vertex);
    layout.attributes = {
        {0, renderer::VertexAttributeType::Float3, offsetof(Vertex, position)},
        {1, renderer::VertexAttributeType::Float2, offsetof(Vertex, uv)},
    };
    return layout;
}

} // namespace

TestAppLayer::TestAppLayer(bool smoke_vulkan, bool enable_renderer, bool smoke_render)
    : Layer("TestAppLayer"),
      m_smoke_vulkan(smoke_vulkan),
      m_enable_renderer(enable_renderer),
      m_smoke_render(smoke_render)
{}

TestAppLayer::~TestAppLayer() = default;

void TestAppLayer::onAttach()
{
    core::Application::get().getLogChannel().info("hello world");

    if (m_enable_renderer) {
        auto surface_source = platform::createSDLVulkanSurfaceSource(core::Application::get().getWindow());
        renderer::RenderDeviceCreateInfo render_device_info;
        render_device_info.application_name = "Test App";
        m_render_device = std::make_unique<renderer::RenderDevice>(
            core::Application::get().getWindow(), std::move(surface_source), render_device_info);

        m_scene = std::make_unique<scene::Scene>();
        m_scene->addSystem<CameraControllerSystem>(scene::SystemStage::Update);
        m_scene->addSystem<RotationSystem>(scene::SystemStage::Update);
        m_scene->addSystem<RenderSystem>(
            scene::SystemStage::RenderExtract,
            *m_render_device,
            core::Application::get().getWindow(),
            ARTI_TEST_COMPUTE_SHADER_PATH,
            ARTI_TEST_MESH_SHADER_PATH,
            ARTI_TEST_COMPOSITE_SHADER_PATH);

        auto vertex_buffer = m_render_device->createVertexBuffer(
            std::as_bytes(std::span{vertices}), static_cast<uint32_t>(vertices.size()), cubeVertexLayout());
        auto index_buffer =
            m_render_device->createIndexBuffer(std::as_bytes(std::span{indices}), static_cast<uint32_t>(indices.size()));

        const ImageData brownie = loadImageRGBA(ARTI_TEST_BROWNIE_TEXTURE_PATH);
        auto texture = m_render_device->createTexture2D(
            brownie.rgba_pixels, brownie.width, brownie.height, renderer::TextureFormat::RGBA8Srgb);

        auto cube = m_scene->createEntity("cube");
        cube.addComponent<MeshComponent>(std::move(vertex_buffer), std::move(index_buffer));
        cube.addComponent<MaterialComponent>(std::move(texture));
        cube.addComponent<RotationComponent>(glm::vec3{0.7f, 1.0f, 0.35f}, 0.85f);
        m_cube_entity = cube;

        auto camera = m_scene->createEntity("camera");
        auto& camera_transform = camera.getComponent<scene::TransformComponent>();
        camera_transform.translation = glm::vec3{2.8f, 2.2f, 3.2f};
        camera_transform.rotation =
            glm::quatLookAt(glm::normalize(-camera_transform.translation), glm::vec3{0.0f, 1.0f, 0.0f});
        camera.addComponent<CameraComponent>();

        auto hierarchy_parent = m_scene->createEntity("hierarchy_parent");
        auto& hierarchy_parent_transform = hierarchy_parent.getComponent<scene::TransformComponent>();
        hierarchy_parent_transform.translation = glm::vec3{1.0f, 2.0f, 3.0f};
        hierarchy_parent_transform.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3{0.0f, 1.0f, 0.0f});
        m_hierarchy_parent_entity = hierarchy_parent;

        auto hierarchy_child = m_scene->createEntity("hierarchy_child");
        m_scene->setParent(hierarchy_child, hierarchy_parent);
        auto& hierarchy_child_transform = hierarchy_child.getComponent<scene::TransformComponent>();
        hierarchy_child_transform.translation = glm::vec3{4.0f, 5.0f, 6.0f};
        m_hierarchy_child_entity = hierarchy_child;

        if (m_smoke_render) {
            m_throw_once_pass = std::make_unique<ThrowOncePass>();
        }
        m_render_frames_remaining = m_smoke_render ? 5 : 0;
        core::Application::get().getLogChannel().info(
            "Rendering MRT cube demo with brownie.png ({}x{})", brownie.width, brownie.height);
        return;
    }

    if (!m_smoke_vulkan) {
        return;
    }

    auto surface_source = platform::createSDLVulkanSurfaceSource(core::Application::get().getWindow());
    renderer::vulkan::VulkanContextCreateInfo context_info;
    context_info.application_name = "Test App";
    renderer::vulkan::VulkanContext context{context_info, *surface_source};
    renderer::vulkan::VulkanSurface surface{context.instance(), *surface_source};
    renderer::vulkan::VulkanDevice device{context.instance(), surface.handle()};
    renderer::vulkan::VulkanAllocator allocator{context, device};

    constexpr std::array<uint32_t, 4> values = {1, 2, 3, 4};
    vk::BufferCreateInfo buffer_info{};
    buffer_info.setSize(sizeof(values))
        .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
        .setSharingMode(vk::SharingMode::eExclusive);

    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    auto buffer = allocator.createBuffer(buffer_info, allocation_info);
    std::memcpy(buffer.map(), values.data(), sizeof(values));
    buffer.flush();
    buffer.unmap();

    vk::ImageCreateInfo image_info{};
    image_info.setImageType(vk::ImageType::e2D)
        .setFormat(vk::Format::eR8G8B8A8Unorm)
        .setExtent({1, 1, 1})
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setInitialLayout(vk::ImageLayout::eUndefined);

    VmaAllocationCreateInfo image_allocation_info{};
    image_allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    auto image = allocator.createImage(image_info, image_allocation_info);
    core::Application::get().getLogChannel().info("Vulkan device and VMA smoke test passed");
}

void TestAppLayer::onDetach()
{
    if (m_render_device) {
        m_render_device->waitIdle();
    }
    m_scene.reset();
    m_render_device.reset();
}

void TestAppLayer::onUpdate(core::Timestep delta_time)
{
    m_delta_time = delta_time;
    if (!m_scene) {
        return;
    }

    m_fixed_timestep.tick(delta_time.getSeconds(), [this](float fixed_dt) {
        scene::UpdateContext fixed_context;
        fixed_context.deltaTime = core::Timestep{fixed_dt};
        fixed_context.fixedDeltaTime = core::Timestep{fixed_dt};
        fixed_context.frameIndex = m_fixed_frame_index++;
        m_scene->runSystems(scene::SystemStage::FixedUpdate, fixed_context);
    });

    scene::UpdateContext context;
    context.deltaTime = delta_time;
    context.fixedDeltaTime = core::Timestep{m_fixed_timestep.fixedDeltaTime()};
    context.frameIndex = m_frame_index;
    m_scene->runSystems(scene::SystemStage::Update, context);
    m_scene->runSystems(scene::SystemStage::LateUpdate, context);

    if (m_smoke_render && m_frame_index == 0) {
        verifyHierarchy();
    }
}

void TestAppLayer::verifyHierarchy()
{
    const auto& child_world = m_scene->getWorldTransform(m_hierarchy_child_entity);
    const auto& parent_transform = m_hierarchy_parent_entity.getComponent<scene::TransformComponent>();
    const auto& child_transform = m_hierarchy_child_entity.getComponent<scene::TransformComponent>();
    const glm::mat4 expected = parent_transform.getTransform() * child_transform.getTransform();
    for (size_t index = 0; index < 16; ++index) {
        if (std::abs(glm::value_ptr(child_world)[index] - glm::value_ptr(expected)[index]) > 1e-5f) {
            throw std::runtime_error("Hierarchy world transform propagation failed.");
        }
    }

    const std::vector<scene::Entity> children = m_scene->getChildren(m_hierarchy_parent_entity);
    if (children.size() != 1 || children.front() != m_hierarchy_child_entity) {
        throw std::runtime_error("Hierarchy child enumeration failed.");
    }

    m_scene->destroyEntity(m_hierarchy_parent_entity);
    if (m_scene->isValid(m_hierarchy_child_entity)) {
        throw std::runtime_error("Hierarchy cascade destruction failed.");
    }
    m_hierarchy_parent_entity = {};
    m_hierarchy_child_entity = {};
}

void TestAppLayer::onRender()
{
    if (!m_scene) {
        return;
    }

    scene::UpdateContext context;
    context.deltaTime = m_delta_time;
    context.fixedDeltaTime = m_delta_time;
    context.frameIndex = m_frame_index;

    if (m_throw_once_pass) {
        renderer::vulkan::VulkanPass* throw_pass = m_throw_once_pass.get();
        m_scene->getSystem<RenderSystem>().prependPass(throw_pass);
        bool caught_expected_failure = false;
        try {
            m_scene->runSystems(scene::SystemStage::RenderExtract, context);
        } catch (const std::runtime_error&) {
            if (!m_throw_once_pass->didThrow()) {
                throw;
            }
            caught_expected_failure = true;
        }
        m_scene->getSystem<RenderSystem>().removePass(throw_pass);
        m_throw_once_pass.reset();
        if (!caught_expected_failure) {
            throw std::logic_error("The Vulkan frame recovery test pass did not throw.");
        }
        m_frame_recovery_awaiting_success = true;
    }

    m_scene->runSystems(scene::SystemStage::RenderExtract, context);
    m_frame_index++;
    if (m_frame_recovery_awaiting_success) {
        core::Application::get().getLogChannel().info("Vulkan frame recording recovery smoke test passed");
        m_frame_recovery_awaiting_success = false;
    }
    if (!m_smoke_render) {
        return;
    }
    if (m_render_frames_remaining == 5) {
        auto replacement_vertices = m_render_device->createVertexBuffer(
            std::as_bytes(std::span{vertices}), static_cast<uint32_t>(vertices.size()), cubeVertexLayout());
        auto replacement_indices =
            m_render_device->createIndexBuffer(std::as_bytes(std::span{indices}), static_cast<uint32_t>(indices.size()));
        m_cube_entity.getComponent<MeshComponent>() = MeshComponent{std::move(replacement_vertices),
                                                                    std::move(replacement_indices)};

        const ImageData brownie = loadImageRGBA(ARTI_TEST_BROWNIE_TEXTURE_PATH);
        auto replacement_texture = m_render_device->createTexture2D(
            brownie.rgba_pixels, brownie.width, brownie.height, renderer::TextureFormat::RGBA8Srgb);
        m_cube_entity.getComponent<MaterialComponent>() = MaterialComponent{std::move(replacement_texture)};
        core::Application::get().getWindow().resize(960, 540);
    }
    if (--m_render_frames_remaining == 0) {
        core::Application::get().getLogChannel().info("Vulkan MRT cube smoke test passed");
        core::Application::get().close();
    }
}

} // namespace arti::test_app
