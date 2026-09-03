#include "render_system.h"

#include "artichoco/core/task/task_system.h"
#include "artichoco/core/window.h"
#include "artichoco/renderer/render_device.h"
#include "artichoco/scene/scene.h"
#include "mrt_composite_pass.h"
#include "mrt_mesh_pass.h"
#include "scene_components.h"
#include "texture_compute_pass.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <utility>

namespace arti::test_app {

RenderSystem::RenderSystem(renderer::RenderDevice& render_device, core::Window& window,
        size_t render_slots, std::filesystem::path compute_shader_path,
        std::filesystem::path mesh_shader_path, std::filesystem::path composite_shader_path)
        : m_render_device(render_device),
          m_window(window),
          m_compute_shader_path(std::move(compute_shader_path)),
          m_mesh_shader_path(std::move(mesh_shader_path)),
          m_composite_shader_path(std::move(composite_shader_path)),
          m_frame_queue(render_slots) {}

RenderSystem::~RenderSystem() = default;

void RenderSystem::onAttach(scene::Scene& scene) {
    if (m_render_thread_started) {
        return;
    }
    if (core::TaskSystem::get().threadCount() < 2) {
        throw std::runtime_error("The render thread requires at least one task worker thread.");
    }
    m_render_thread_task = core::TaskSystem::get().submitPinned(1, [this] { renderThreadLoop(); });
    m_render_thread_started = true;
}

void RenderSystem::onDetach(scene::Scene& scene) {
    m_shutdown_requested.store(true, std::memory_order_release);
    m_frame_queue.shutdown();
    core::TaskSystem::get().wait(m_render_thread_task);
    m_render_device.waitIdle();
}

void RenderSystem::onUpdate(scene::Scene& scene, const scene::UpdateContext& context) {
    m_elapsed_time += context.deltaTime.getSeconds();

    ensurePasses(scene);

    auto cameras = scene.view<scene::TransformComponent, CameraComponent>();
    if (cameras.begin() == cameras.end()) {
        return;
    }
    const auto camera_entity = cameras.front();
    const auto& camera_transform = cameras.get<scene::TransformComponent>(camera_entity);
    const auto& camera = cameras.get<CameraComponent>(camera_entity);

    RenderFrameData frame_data;
    frame_data.time = m_elapsed_time;
    frame_data.frame_index = context.frameIndex;

    const float aspect = static_cast<float>(m_window.getFramebufferWidth()) /
                         static_cast<float>(m_window.getFramebufferHeight());
    frame_data.projection =
            glm::perspective(glm::radians(camera.fov), aspect, camera.near_plane, camera.far_plane);
    frame_data.view = glm::inverse(camera_transform.getTransform());

    for (auto [entity, transform, mesh, material]:
            scene.view<scene::TransformComponent, MeshComponent, MaterialComponent>().each()) {
        RenderDrawCommand draw;
        draw.vertex_buffer = mesh.mesh.vertexBufferPtr();
        draw.index_buffer = mesh.mesh.indexBufferPtr();
        draw.base_color_texture = material.material.texturePtr();
        draw.model_matrix = transform.getTransform();
        frame_data.draws.push_back(std::move(draw));
    }

    auto& slot = m_frame_queue.acquireWriteSlot();
    slot = std::move(frame_data);
    m_last_draw_count.store(slot.draws.size(), std::memory_order_release);
    m_frame_queue.publish();
}

void RenderSystem::renderThreadLoop() {
    while (!m_shutdown_requested.load(std::memory_order_acquire)) {
        const RenderFrameData* frame_data = m_frame_queue.waitForNext();
        if (frame_data == nullptr) {
            break;
        }
        try {
            const std::vector<renderer::RenderPass*> passes = snapshotPasses();
            m_texture_compute_pass->applyFrameData(*frame_data);
            m_mrt_mesh_pass->applyFrameData(*frame_data);
            m_render_device.renderFrame(
                    std::span<renderer::RenderPass* const>{ passes.data(), passes.size() });
        } catch (...) {
            std::lock_guard lock{ m_error_mutex };
            m_last_render_error = std::current_exception();
        }
        m_frame_queue.releaseReadSlot();
    }
}

std::vector<renderer::RenderPass*> RenderSystem::snapshotPasses() {
    std::lock_guard lock{ m_pass_mutex };
    return m_passes;
}

void RenderSystem::ensurePasses(scene::Scene& scene) {
    if (m_passes_initialized) {
        return;
    }

    auto materials = scene.view<MaterialComponent>();
    if (materials.begin() == materials.end()) {
        return;
    }
    const std::shared_ptr<renderer::Texture2D> texture =
            materials.get<MaterialComponent>(materials.front()).material.texturePtr();
    m_texture_compute_pass = std::make_unique<TextureComputePass>(texture, m_compute_shader_path);
    m_mrt_mesh_pass = std::make_unique<MrtMeshPass>(*m_texture_compute_pass, m_mesh_shader_path);
    m_mrt_composite_pass =
            std::make_unique<MrtCompositePass>(*m_mrt_mesh_pass, m_composite_shader_path);

    std::lock_guard lock{ m_pass_mutex };
    std::vector<renderer::RenderPass*> external_passes = std::move(m_passes);
    m_passes = {
        m_texture_compute_pass.get(),
        m_mrt_mesh_pass.get(),
        m_mrt_composite_pass.get(),
    };
    m_passes.insert(m_passes.begin(), external_passes.begin(), external_passes.end());
    m_passes_initialized = true;
}

void RenderSystem::prependPass(renderer::RenderPass* pass) {
    std::lock_guard lock{ m_pass_mutex };
    m_passes.erase(std::remove(m_passes.begin(), m_passes.end(), pass), m_passes.end());
    m_passes.insert(m_passes.begin(), pass);
}

void RenderSystem::removePass(renderer::RenderPass* pass) noexcept {
    std::lock_guard lock{ m_pass_mutex };
    m_passes.erase(std::remove(m_passes.begin(), m_passes.end(), pass), m_passes.end());
}

void RenderSystem::waitForFrameComplete() { m_frame_queue.waitUntilDrained(); }

std::exception_ptr RenderSystem::consumeRenderError() {
    std::lock_guard lock{ m_error_mutex };
    return std::exchange(m_last_render_error, {});
}

size_t RenderSystem::lastDrawCount() const noexcept {
    return m_last_draw_count.load(std::memory_order_acquire);
}

} // namespace arti::test_app
