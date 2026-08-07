#pragma once
#include "artichoco/core/timestep.h"

#include <cstdint>

namespace arti::scene {

class Scene;

enum class SystemStage : std::uint8_t {
    FixedUpdate,
    Update,
    LateUpdate,
    RenderExtract
};

struct UpdateContext {
    core::Timestep deltaTime{};
    core::Timestep fixedDeltaTime{};
    std::uint64_t frameIndex{0};
};

class SceneSystem {
public:
    virtual ~SceneSystem() = default;

    virtual void onAttach(Scene&) {}
    virtual void onDetach(Scene&) {}
    virtual void onUpdate(Scene&, const UpdateContext&) {}
};

} // namespace arti::scene
