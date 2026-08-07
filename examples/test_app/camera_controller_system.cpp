#include "camera_controller_system.h"

#include "artichoco/core/io/input.h"
#include "artichoco/scene/scene.h"
#include "scene_components.h"

#include <algorithm>
#include <glm/gtc/quaternion.hpp>

namespace arti::test_app {
namespace {

constexpr float sensitivity = 0.0025f;
constexpr float move_speed = 5.0f;
constexpr float max_pitch = glm::radians(89.0f);

} // namespace

void CameraControllerSystem::onUpdate(scene::Scene& scene, const scene::UpdateContext& context)
{
    const bool right_pressed = core::Input::isMouseButtonPressed(core::MouseCode::Right);
    if (right_pressed && !m_look_active) {
        beginLook(scene);
    } else if (!right_pressed && m_look_active) {
        endLook();
    }
    if (!m_look_active) {
        return;
    }

    if (!m_orientation_initialized) {
        for (auto [entity, transform, camera] : scene.view<scene::TransformComponent, CameraComponent>().each()) {
            const glm::vec3 euler = glm::eulerAngles(transform.rotation);
            m_pitch = euler.x;
            m_yaw = euler.y;
            m_orientation_initialized = true;
        }
    }

    const glm::vec2 delta = core::Input::getMouseDelta();
    m_yaw -= delta.x * sensitivity;
    m_pitch -= delta.y * sensitivity;
    m_pitch = std::clamp(m_pitch, -max_pitch, max_pitch);

    const glm::quat rotation = glm::quat(glm::vec3{m_pitch, m_yaw, 0.0f});
    const glm::vec3 forward = rotation * glm::vec3{0.0f, 0.0f, -1.0f};
    const glm::vec3 right = rotation * glm::vec3{1.0f, 0.0f, 0.0f};

    glm::vec3 movement{0.0f};
    if (core::Input::isKeyPressed(core::KeyCode::W)) {
        movement += forward;
    }
    if (core::Input::isKeyPressed(core::KeyCode::S)) {
        movement -= forward;
    }
    if (core::Input::isKeyPressed(core::KeyCode::A)) {
        movement -= right;
    }
    if (core::Input::isKeyPressed(core::KeyCode::D)) {
        movement += right;
    }
    if (glm::dot(movement, movement) > 0.0f) {
        movement = glm::normalize(movement) * (move_speed * context.deltaTime.getSeconds());
    }

    for (auto [entity, transform, camera] : scene.view<scene::TransformComponent, CameraComponent>().each()) {
        transform.rotation = rotation;
        transform.translation += movement;
    }
}

void CameraControllerSystem::beginLook(scene::Scene& scene)
{
    if (scene.view<scene::TransformComponent, CameraComponent>().begin() ==
        scene.view<scene::TransformComponent, CameraComponent>().end()) {
        return;
    }
    core::Input::setCursorMode(core::CursorMode::Locked);
    core::Input::setRawMouseMotion(true);
    m_look_active = true;
}

void CameraControllerSystem::endLook() noexcept
{
    core::Input::setCursorMode(core::CursorMode::Normal);
    core::Input::setRawMouseMotion(false);
    m_look_active = false;
}

} // namespace arti::test_app
