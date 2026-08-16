#include "artichoco/scene/scene.h"

#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireEqual(const glm::mat4& actual, const glm::mat4& expected, const char* message)
{
    for (size_t index = 0; index < 16; ++index) {
        if (std::abs(glm::value_ptr(actual)[index] - glm::value_ptr(expected)[index]) > 1e-5f) {
            throw std::runtime_error(message);
        }
    }
}

void runSmokeTest()
{
    arti::scene::Scene scene;
    auto root = scene.createEntity("root");
    auto child = scene.createEntity("child");
    auto grandchild = scene.createEntity("grandchild");

    auto& root_transform = root.getComponent<arti::scene::TransformComponent>();
    auto& child_transform = child.getComponent<arti::scene::TransformComponent>();
    auto& grandchild_transform = grandchild.getComponent<arti::scene::TransformComponent>();
    root_transform.translation = {1.0f, 0.0f, 0.0f};
    child_transform.translation = {0.0f, 2.0f, 0.0f};
    grandchild_transform.translation = {0.0f, 0.0f, 3.0f};

    scene.setParent(child, root);
    scene.setParent(grandchild, child);
    scene.updateWorldTransforms();

    for (auto [handle, world]: scene.view<arti::scene::WorldTransformComponent>().each()) {
        static_assert(std::is_const_v<std::remove_reference_t<decltype(world)>>);
        (void) handle;
        (void) world;
    }

    require(scene.getParent(child) == root && scene.getParent(grandchild) == child,
            "Scene hierarchy returned the wrong parent.");
    const auto children = scene.getChildren(root);
    require(children.size() == 1 && children.front() == child,
            "Scene hierarchy returned the wrong children.");

    const glm::mat4 expected_world = root_transform.getTransform() * child_transform.getTransform() *
                                     grandchild_transform.getTransform();
    requireEqual(scene.getWorldTransform(grandchild), expected_world,
            "Scene hierarchy calculated the wrong world transform.");

    root_transform.translation = {4.0f, 5.0f, 6.0f};
    scene.updateWorldTransforms();
    const glm::mat4 updated_world = root_transform.getTransform() *
                                    child_transform.getTransform() *
                                    grandchild_transform.getTransform();
    requireEqual(scene.getWorldTransform(grandchild), updated_world,
            "A parent transform change did not propagate to its descendants.");

    const arti::scene::Entity readonly_grandchild = grandchild;
    require(!readonly_grandchild
                     .getComponent<arti::scene::WorldTransformComponent>()
                     .dirty,
            "The world transform remained dirty after updating the hierarchy.");

    bool rejected_cycle = false;
    try {
        scene.setParent(root, grandchild);
    } catch (const std::invalid_argument&) {
        rejected_cycle = true;
    }
    require(rejected_cycle, "Scene hierarchy accepted a parent cycle.");

    scene.detachFromParent(grandchild);
    require(!scene.getParent(grandchild), "Scene hierarchy did not detach an Entity.");
    requireEqual(scene.getWorldTransform(grandchild), grandchild_transform.getTransform(),
            "Detached Entity retained its former parent transform.");

    scene.setParent(grandchild, child);
    const auto root_id = root.getUUID();
    const auto child_id = child.getUUID();
    const auto grandchild_id = grandchild.getUUID();
    scene.destroyEntity(root);
    require(!scene.containsEntity(root_id) && !scene.containsEntity(child_id) &&
                    !scene.containsEntity(grandchild_id),
            "Destroying a hierarchy root did not destroy its descendants.");
}

} // namespace

int main()
{
    try {
        runSmokeTest();
        std::cout << "Scene hierarchy smoke test passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Scene hierarchy smoke test failed: " << exception.what() << '\n';
        return 1;
    }
}
