#include "render_system.h"

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
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace arti::test_app {

RenderSystem::RenderSystem(renderer::RenderDevice& render_device,
                           core::Window& window,
                           std::filesystem::path compute_shader_path,
                           std::filesystem::path mesh_shader_path,
                           std::filesystem::path composite_shader_path)
    : m_render_device(render_device),
      m_window(window),
      m_compute_shader_path(std::move(compute_shader_path)),
      m_mesh_shader_path(std::move(mesh_shader_path)),
      m_composite_shader_path(std::move(composite_shader_path))
{}

RenderSystem::~RenderSystem() = default;

void RenderSystem::onUpdate(scene::Scene& scene, const scene::UpdateContext& context)
{
    m_elapsed_time += context.deltaTime.getSeconds();

    if (!m_passes_initialized) {
        auto materials = scene.view<MaterialComponent>();
        if (materials.empty()) {
            return;
        }
        const renderer::Texture2D& texture =
            materials.get<MaterialComponent>(materials.front()).material.baseColorTexture();
        m_texture_compute_pass = std::make_unique<TextureComputePass>(texture, m_compute_shader_path);
        m_mrt_mesh_pass = std::make_unique<MrtMeshPass>(*m_texture_compute_pass, m_mesh_shader_path);
        m_mrt_composite_pass = std::make_unique<MrtCompositePass>(*m_mrt_mesh_pass, m_composite_shader_path);
        std::vector<renderer::vulkan::VulkanPass*> external_passes = std::move(m_passes);
        m_passes = {
            m_texture_compute_pass.get(),
            m_mrt_mesh_pass.get(),
            m_mrt_composite_pass.get(),
        };
        m_passes.insert(m_passes.begin(), external_passes.begin(), external_passes.end());
        m_passes_initialized = true;
    }

    auto cameras = scene.view<scene::TransformComponent, CameraComponent>();
    if (cameras.begin() == cameras.end()) {
        return;
    }
    const auto camera_entity = cameras.front();
    const auto& camera_transform = cameras.get<scene::TransformComponent>(camera_entity);
    const auto& camera = cameras.get<CameraComponent>(camera_entity);

    const float aspect = static_cast<float>(m_window.getFramebufferWidth()) /
                         static_cast<float>(m_window.getFramebufferHeight());
    glm::mat4 projection = glm::perspective(glm::radians(camera.fov), aspect, camera.near_plane, camera.far_plane);
    projection[1][1] *= -1.0f;
    const glm::mat4 view = glm::inverse(camera_transform.getTransform());

    for (auto [entity, transform, mesh, material] :
         scene.view<scene::TransformComponent, MeshComponent, MaterialComponent>().each()) {
        const glm::mat4 mvp = projection * view * transform.getTransform();
        std::array<float, 16> transform_values;
        std::memcpy(transform_values.data(), glm::value_ptr(mvp), sizeof(transform_values));

        m_texture_compute_pass->setSource(material.material.baseColorTexture());
        m_texture_compute_pass->setTime(m_elapsed_time);
        m_mrt_mesh_pass->setGeometry(mesh.mesh.vertexBuffer(), mesh.mesh.indexBuffer());
        m_mrt_mesh_pass->setTransform(transform_values);
    }

    m_render_device.renderFrame(
        std::span<renderer::vulkan::VulkanPass* const>{m_passes.data(), m_passes.size()});
}

void RenderSystem::prependPass(renderer::vulkan::VulkanPass* pass)
{
    m_passes.erase(std::remove(m_passes.begin(), m_passes.end(), pass), m_passes.end());
    m_passes.insert(m_passes.begin(), pass);
}

void RenderSystem::removePass(renderer::vulkan::VulkanPass* pass) noexcept
{
    m_passes.erase(std::remove(m_passes.begin(), m_passes.end(), pass), m_passes.end());
}

std::span<renderer::vulkan::VulkanPass* const> RenderSystem::passes() const noexcept
{
    return {m_passes.data(), m_passes.size()};
}

} // namespace arti::test_app
