#pragma once
#include "artichoco/scene/system.h"
#include "scene_components.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace arti::test_app {

class RotationSystem final : public scene::SceneSystem {
public:
    void onUpdate(scene::Scene& scene, const scene::UpdateContext& context) override
    {
        const float angle = context.deltaTime.getSeconds();
        for (auto [entity, transform, rotation] :
             scene.view<scene::TransformComponent, RotationComponent>().each()) {
            const glm::quat step = glm::angleAxis(angle * rotation.speed, glm::normalize(rotation.axis));
            transform.rotation = step * transform.rotation;
        }
    }
};

} // namespace arti::test_app
