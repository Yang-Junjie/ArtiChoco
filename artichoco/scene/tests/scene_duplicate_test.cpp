// Scene::duplicateEntity() 的测试。
//
// 这条路径的分支都在「身份」上：谁拿新 UUID、谁的父级要重映射、哪些组件跟着走。全都是
// 一眼看过去像对的、错了也不崩的东西 —— 父级重映射写错只是副本的孩子悄悄挂到了源身上，
// 忘了 indexEntity() 只是副本按 UUID 查不到。所以这里每一条断言都盯着一种「不报错的错」。

#include "artichoco/core/log.h"
#include "artichoco/scene/components.h"
#include "artichoco/scene/scene.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using arti::scene::Entity;
using arti::scene::IDComponent;
using arti::scene::ParentComponent;
using arti::scene::Scene;
using arti::scene::TagComponent;
using arti::scene::TransformComponent;

// 注册过拷贝的组件。刻意带两个会堆分配的字段：拷贝写错成读悬空引用时，这两个比一个 int
// 更容易露出马脚。
struct PayloadComponent {
    std::string text;
    std::vector<int> numbers;
};

// 刻意**不**注册。副本应该缺它，而且只记一条 warn、不崩。
struct UnregisteredComponent {
    int value{ 0 };
};

bool require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "scene_duplicate_test: " << message << '\n';
    }
    return condition;
}

size_t entityCount(Scene& scene) {
    size_t count = 0;
    for ([[maybe_unused]] auto entry: scene.view<IDComponent>().each()) {
        ++count;
    }
    return count;
}

Entity childByTag(Scene& scene, Entity parent, std::string_view tag) {
    for (const Entity& child: scene.getChildren(parent)) {
        if (child.getComponent<TagComponent>().tag == tag) {
            return child;
        }
    }
    return {};
}

int testDuplicateLoneEntity() {
    Scene scene;
    Entity source = scene.createEntity("Cube");
    source.getComponent<TransformComponent>().translation = glm::vec3{ 1.0f, 2.0f, 3.0f };

    const Entity copy = scene.duplicateEntity(source);

    if (!require(copy.isValid(), "the copy is not a valid entity")) {
        return 1;
    }
    if (!require(copy.getUUID() != source.getUUID(), "the copy reused the source UUID")) {
        return 1;
    }
    if (!require(copy.getComponent<TagComponent>().tag == "Cube", "the tag was not copied")) {
        return 1;
    }
    if (!require(copy.getComponent<TransformComponent>().translation ==
                         glm::vec3{ 1.0f, 2.0f, 3.0f },
                "the transform was not copied")) {
        return 1;
    }
    if (!require(!copy.getComponent<ParentComponent>().parent_id.isValid(),
                "a copy of a root entity got a parent")) {
        return 1;
    }
    if (!require(entityCount(scene) == 2, "duplicating one entity did not produce exactly one")) {
        return 1;
    }
    // 副本必须能被 UUID 找回来 —— 忘了 indexEntity() 的话上面全过、这一条挂。
    if (!require(scene.findEntity(copy.getUUID()) == copy, "the copy is not in the lookup table")) {
        return 1;
    }
    if (!require(scene.findEntity(source.getUUID()) == source, "the source went missing")) {
        return 1;
    }
    return 0;
}

int testDuplicateSubtree() {
    Scene scene;
    Entity root = scene.createEntity("Root");
    Entity child_a = scene.createEntity("ChildA");
    Entity child_b = scene.createEntity("ChildB");
    Entity grandchild = scene.createEntity("Grandchild");
    scene.setParent(child_a, root);
    scene.setParent(child_b, root);
    scene.setParent(grandchild, child_a);

    const Entity copy = scene.duplicateEntity(root);

    if (!require(entityCount(scene) == 8, "the whole subtree was not duplicated")) {
        return 1;
    }
    if (!require(scene.getChildren(copy).size() == 2, "the copy has the wrong number of children")) {
        return 1;
    }
    const Entity copied_child_a = childByTag(scene, copy, "ChildA");
    if (!require(copied_child_a.isValid(), "the copied subtree is missing ChildA")) {
        return 1;
    }
    if (!require(copied_child_a.getUUID() != child_a.getUUID(),
                "the copied child reused the source UUID")) {
        return 1;
    }
    // 孙子必须挂在**副本的** ChildA 下面，不是原来那个 —— 父级重映射写错时这一条挂。
    if (!require(scene.getChildren(copied_child_a).size() == 1,
                "the grandchild did not follow the copy")) {
        return 1;
    }
    // 源子树一点没被动过。
    if (!require(scene.getChildren(root).size() == 2, "the source subtree gained children")) {
        return 1;
    }
    if (!require(scene.getChildren(child_a).size() == 1, "the source lost its grandchild")) {
        return 1;
    }
    return 0;
}

int testDuplicateChildBecomesSibling() {
    Scene scene;
    Entity parent = scene.createEntity("Parent");
    Entity child = scene.createEntity("Child");
    scene.setParent(child, parent);

    const Entity copy = scene.duplicateEntity(child);

    // 子树的根的父级在子树外面，所以查不到重映射、保持原样 —— 副本成为源的兄弟。
    if (!require(copy.getComponent<ParentComponent>().parent_id == parent.getUUID(),
                "the copy is not a sibling of the source")) {
        return 1;
    }
    if (!require(scene.getChildren(parent).size() == 2, "the parent did not gain the copy")) {
        return 1;
    }
    return 0;
}

int testCopiesRegisteredComponentsOnly() {
    Scene::registerComponentCopy<PayloadComponent>();

    Scene scene;
    Entity source = scene.createEntity("WithPayload");
    auto& payload = source.addComponent<PayloadComponent>();
    payload.text = std::string(64, 'x');
    payload.numbers.assign(64, 7);
    source.addComponent<UnregisteredComponent>().value = 7;

    const Entity copy = scene.duplicateEntity(source);

    if (!require(copy.hasComponent<PayloadComponent>(),
                "a registered component was not copied")) {
        return 1;
    }
    if (!require(copy.getComponent<PayloadComponent>().text == std::string(64, 'x'),
                "the copied component has the wrong string")) {
        return 1;
    }
    if (!require(copy.getComponent<PayloadComponent>().numbers == std::vector<int>(64, 7),
                "the copied component has the wrong vector")) {
        return 1;
    }
    // 没注册的类型必须被跳过（记 warn），而不是崩、也不是半个组件。
    if (!require(!copy.hasComponent<UnregisteredComponent>(),
                "an unregistered component was copied anyway")) {
        return 1;
    }
    if (!require(source.getComponent<UnregisteredComponent>().value == 7,
                "the source's unregistered component was disturbed")) {
        return 1;
    }
    return 0;
}

// 复制副本的副本。这一条盯的是查找表：每一轮的新 UUID 都要登记进去，否则下一轮
// `containsEntity()` 看不见它，去重循环就可能发出一个已经在用的 UUID。
int testDuplicatingACopy() {
    Scene::registerComponentCopy<PayloadComponent>();

    Scene scene;
    Entity source = scene.createEntity("Seed");
    Entity child = scene.createEntity("Leaf");
    scene.setParent(child, source);
    source.addComponent<PayloadComponent>().text = std::string(64, 'x');

    Entity current = source;
    for (int generation = 0; generation < 32; ++generation) {
        const Entity next = scene.duplicateEntity(current);
        if (!require(next.getUUID() != current.getUUID(),
                    "a chained duplicate reused its source UUID")) {
            return 1;
        }
        if (!require(scene.findEntity(next.getUUID()) == next,
                    "a chained duplicate is not in the lookup table")) {
            return 1;
        }
        // 名字原样照抄 —— 消歧是编辑器的策略，不该悄悄跑进场景里来。
        if (!require(next.getComponent<TagComponent>().tag == "Seed",
                    "duplicateEntity() renamed the copy")) {
            return 1;
        }
        if (!require(next.getComponent<PayloadComponent>().text == std::string(64, 'x'),
                    "a chained duplicate lost its payload")) {
            return 1;
        }
        if (!require(scene.getChildren(next).size() == 1,
                    "a chained duplicate lost its child")) {
            return 1;
        }
        current = next;
    }

    // 33 代 × 每代 2 个实体。少一个就说明某一轮的 UUID 撞了、或者子树没跟上。
    if (!require(entityCount(scene) == 66, "chained duplication produced the wrong entity count")) {
        return 1;
    }
    return 0;
}

int testWorldTransformsFollowTheCopy() {
    Scene scene;
    Entity parent = scene.createEntity("Parent");
    Entity child = scene.createEntity("Child");
    parent.getComponent<TransformComponent>().translation = glm::vec3{ 10.0f, 0.0f, 0.0f };
    child.getComponent<TransformComponent>().translation = glm::vec3{ 1.0f, 0.0f, 0.0f };
    scene.setParent(child, parent);
    scene.updateWorldTransforms();

    Entity copy = scene.duplicateEntity(parent);
    copy.getComponent<TransformComponent>().translation = glm::vec3{ 20.0f, 0.0f, 0.0f };
    scene.updateWorldTransforms();

    const Entity copied_child = childByTag(scene, copy, "Child");
    if (!require(copied_child.isValid(), "the copied parent has no child")) {
        return 1;
    }
    // 副本的孩子跟着副本走（20 + 1），源的孩子不动（10 + 1）。缓存的世界变换没标脏、
    // 或者父级重映射写错，这两条里必有一条挂。
    if (!require(scene.getWorldTransform(copied_child)[3].x == 21.0f,
                "the copied child's world transform did not follow the copy")) {
        return 1;
    }
    if (!require(scene.getWorldTransform(child)[3].x == 11.0f,
                "moving the copy disturbed the source's world transform")) {
        return 1;
    }
    return 0;
}

int testRejectsForeignAndInvalidEntities() {
    Scene scene;
    Scene other;
    Entity foreign = other.createEntity("Foreign");

    try {
        scene.duplicateEntity(foreign);
        return require(false, "duplicating an entity from another Scene was allowed") ? 0 : 1;
    } catch (const std::invalid_argument&) {
    }

    try {
        scene.duplicateEntity(Entity{});
        return require(false, "duplicating a default-constructed Entity was allowed") ? 0 : 1;
    } catch (const std::invalid_argument&) {
    }
    return 0;
}

int run() {
    if (testDuplicateLoneEntity() != 0) {
        return 1;
    }
    if (testDuplicateSubtree() != 0) {
        return 1;
    }
    if (testDuplicateChildBecomesSibling() != 0) {
        return 1;
    }
    if (testCopiesRegisteredComponentsOnly() != 0) {
        return 1;
    }
    if (testDuplicatingACopy() != 0) {
        return 1;
    }
    if (testWorldTransformsFollowTheCopy() != 0) {
        return 1;
    }
    if (testRejectsForeignAndInvalidEntities() != 0) {
        return 1;
    }

    std::cout << "scene_duplicate_test: ok\n";
    return 0;
}

} // namespace

int main() {
    arti::core::Logger::init();
    const int result = run();
    arti::core::Logger::shutdown();
    return result;
}
