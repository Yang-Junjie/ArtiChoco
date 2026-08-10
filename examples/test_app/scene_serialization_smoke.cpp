#include "artichoco/scene/scene.h"
#include "artichoco/scene/scene_serialization_registry.h"
#include "artichoco/scene/scene_serializer.h"

#include <cmath>

#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

struct TestComponent {
    int value{0};
    std::string label;
};

class TestComponentSerialization final : public arti::scene::ComponentSerialization<TestComponent> {
public:
    YAML::Node serialize(const TestComponent& component) const override
    {
        YAML::Node node;
        node["Value"] = component.value;
        node["Label"] = component.label;
        return node;
    }

    TestComponent deserialize(const YAML::Node& node) const override
    {
        if (!node || !node.IsMap() || !node["Value"] || !node["Label"]) {
            throw std::invalid_argument("Test component data is invalid.");
        }
        return TestComponent{
            .value = node["Value"].as<int>(),
            .label = node["Label"].as<std::string>(),
        };
    }
};

class TemporarySceneFile {
public:
    TemporarySceneFile()
        : path(std::filesystem::temp_directory_path() /
               ("artichoco_scene_" + arti::core::UUID::generate().toString() + ".yaml"))
    {}

    ~TemporarySceneFile()
    {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    std::filesystem::path path;
};

void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void runSmokeTest()
{
    arti::scene::SceneSerializationRegistry registry;
    registry.registerComponent<TestComponent>("test.component", std::make_unique<TestComponentSerialization>());
    arti::scene::SceneSerializer serializer{registry};

    arti::scene::Scene source;
    auto parent = source.createEntity("parent");
    parent.getComponent<arti::scene::TransformComponent>().translation = {1.0f, 2.0f, 3.0f};
    parent.addComponent<TestComponent>(TestComponent{42, "developer component"});

    auto child = source.createEntity("child");
    child.getComponent<arti::scene::TransformComponent>().translation = {4.0f, 5.0f, 6.0f};
    source.setParent(child, parent);

    TemporarySceneFile file;
    serializer.save(source, file.path);

    arti::scene::Scene restored;
    serializer.load(file.path, restored);

    const auto restored_parent = restored.findEntity(parent.getUUID());
    const auto restored_child = restored.findEntity(child.getUUID());
    require(restored_parent && restored_child, "Scene serialization lost an Entity.");
    require(restored_parent.getComponent<arti::scene::TagComponent>().tag == "parent",
            "Scene serialization changed an Entity tag.");
    require(restored.getParent(restored_child) == restored_parent,
            "Scene serialization did not restore the hierarchy.");

    const auto& component = restored_parent.getComponent<TestComponent>();
    require(component.value == 42 && component.label == "developer component",
            "Scene serialization did not restore a developer Component.");

    const glm::mat4 expected_world = restored_parent.getComponent<arti::scene::TransformComponent>().getTransform() *
                                     restored_child.getComponent<arti::scene::TransformComponent>().getTransform();
    const glm::mat4& actual_world = restored.getWorldTransform(restored_child);
    for (size_t index = 0; index < 16; ++index) {
        require(std::abs(glm::value_ptr(expected_world)[index] - glm::value_ptr(actual_world)[index]) <= 1e-5f,
                "Scene serialization did not rebuild world transforms.");
    }
}

} // namespace

int main()
{
    try {
        runSmokeTest();
        std::cout << "Scene serialization smoke test passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Scene serialization smoke test failed: " << exception.what() << '\n';
        return 1;
    }
}
