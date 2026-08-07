#pragma once
#include "artichoco/scene/system.h"

#include <glm/glm.hpp>

namespace arti::test_app {

class CameraControllerSystem final : public scene::SceneSystem {
public:
    void onUpdate(scene::Scene& scene, const scene::UpdateContext& context) override;

private:
    void beginLook(scene::Scene& scene);
    void endLook() noexcept;

    bool m_look_active{false};
    bool m_orientation_initialized{false};
    float m_yaw{0.0f};
    float m_pitch{0.0f};
};

} // namespace arti::test_app
