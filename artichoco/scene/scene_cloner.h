#pragma once

#include <cstddef>
#include <entt/entt.hpp>
#include <type_traits>
#include <unordered_map>

namespace arti::scene {

class Scene;

class SceneCloner {
public:
    template<typename Component>
    static void registerComponent() {
        static_assert(std::is_copy_constructible_v<Component>,
                "Scene components must be copy-constructible to support scene cloning.");
        registerCopyInto<Component>(copyRegistry());
    }

    static void clone(const Scene& source, Scene& destination);

    // 把 source 身上所有**注册过拷贝**的组件复制到 destination。两个实体可以在同一个
    // registry 里 —— `Scene::duplicateEntity()` 正是这么用的。那条路径安全的前提是 EnTT
    // 默认的分页存储「插入引起扩容时不会让已有的组件引用失效」（third_party/entt 的
    // docs/md/entity.md，Pointer stability 一节）。哪天给组件换了存储策略，
    // 下面那个 `emplace(dst, get(src))` 就要先把值取出来再插。
    //
    // 返回值是「source 有、但没注册过拷贝」的组件类型数：副本会缺这些字段。调用方负责
    // 报出去，这里不打日志 —— 按实体打会在复制一棵大子树时刷屏。
    static size_t copyComponents(
            entt::registry& registry, entt::entity source, entt::entity destination);

private:
    using ComponentCopyFn = void (*)(const entt::registry&, entt::registry&, entt::entity,entt::entity);

    template<typename Component>
    static void registerCopyInto(std::unordered_map<entt::id_type, ComponentCopyFn>& registry) {
        registry.insert_or_assign(entt::type_hash<Component>::value(),
                [](const entt::registry& source, entt::registry& destination,
                   entt::entity source_entity, entt::entity destination_entity)
                {
                    destination.emplace<Component>(destination_entity,source.get<Component>(source_entity));
                });
    }

    static std::unordered_map<entt::id_type, ComponentCopyFn>& copyRegistry();
};

} // namespace arti::scene
