#include "artichoco/scene/scene.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct TestComponent {
    int value{0};
    std::string label;
};

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void runSmokeTest()
{
    arti::scene::SceneCloner::registerComponent<TestComponent>();

    arti::scene::Scene source;
    auto parent = source.createEntity("parent");
    parent.addComponent<TestComponent>(TestComponent{42, "copied component"});

    auto child = source.createEntity("child");
    source.setParent(child, parent);

    arti::scene::Scene destination;
    const auto stale = destination.createEntity("stale");
    const auto stale_id = stale.getUUID();
    arti::scene::SceneCloner::clone(source, destination);

    require(!destination.containsEntity(stale_id),
            "Scene cloning did not clear destination entities.");

    const auto cloned_parent = destination.findEntity(parent.getUUID());
    const auto cloned_child = destination.findEntity(child.getUUID());
    require(cloned_parent && cloned_child, "Scene cloning lost an Entity or its UUID index.");
    require(destination.getParent(cloned_child) == cloned_parent,
            "Scene cloning did not preserve the hierarchy.");

    const auto& cloned_component = cloned_parent.getComponent<TestComponent>();
    require(cloned_component.value == 42 && cloned_component.label == "copied component",
            "Scene cloning did not copy a registered Component.");

    parent.getComponent<TestComponent>().value = 7;
    source.destroyEntity(parent);
    require(cloned_parent && cloned_parent.getComponent<TestComponent>().value == 42,
            "The cloned Scene still depends on source storage.");

    bool rejected_self_clone = false;
    try {
        arti::scene::SceneCloner::clone(destination, destination);
    } catch (const std::invalid_argument&) {
        rejected_self_clone = true;
    }
    require(rejected_self_clone, "Scene cloning accepted the same Scene as source and destination.");
}

} // namespace

int main()
{
    try {
        runSmokeTest();
        std::cout << "Scene cloner smoke test passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Scene cloner smoke test failed: " << exception.what() << '\n';
        return 1;
    }
}
