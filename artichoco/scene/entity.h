#pragma once
#include "components.h"

#include <entt/entt.hpp>

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace arti::scene {

class Scene;

namespace detail {

template <typename Component>
inline constexpr bool isRequiredComponent =
    std::is_same_v<std::remove_cvref_t<Component>, IDComponent> ||
    std::is_same_v<std::remove_cvref_t<Component>, TagComponent> ||
    std::is_same_v<std::remove_cvref_t<Component>, TransformComponent>;

} // namespace detail

class Entity {
public:
    Entity() = default;
    Entity(const Entity&) = default;
    Entity& operator=(const Entity&) = default;

    template <typename Component>
    bool hasComponent() const noexcept
    {
        return isValid() && m_registry->all_of<Component>(m_handle);
    }

    template <typename Component, typename... Args>
    Component& addComponent(Args&&... args)
    {
        requireValid();
        if (m_registry->all_of<Component>(m_handle)) {
            throw std::logic_error("The Entity already has the requested Component.");
        }
        return m_registry->emplace<Component>(m_handle, std::forward<Args>(args)...);
    }

    template <typename Component>
        requires(!std::is_same_v<std::remove_cvref_t<Component>, IDComponent>)
    Component& getComponent()
    {
        requireComponent<Component>();
        return m_registry->get<Component>(m_handle);
    }

    template <typename Component>
    const Component& getComponent() const
    {
        requireComponent<Component>();
        return std::as_const(*m_registry).get<Component>(m_handle);
    }

    template <typename Component>
    bool removeComponent()
    {
        static_assert(!detail::isRequiredComponent<Component>,
                      "IDComponent, TagComponent, and TransformComponent are required and cannot be removed.");
        requireValid();
        return m_registry->remove<Component>(m_handle) != 0;
    }

    entt::entity getHandle() const noexcept
    {
        return m_handle;
    }

    core::UUID getUUID() const
    {
        return getComponent<IDComponent>().id;
    }

    bool isValid() const noexcept
    {
        return m_registry != nullptr && m_handle != entt::null && m_registry->valid(m_handle);
    }

    explicit operator bool() const noexcept
    {
        return isValid();
    }

    bool operator==(const Entity&) const noexcept = default;

private:
    friend class Scene;

    Entity(entt::entity handle, entt::registry& registry) noexcept
        : m_handle(handle),
          m_registry(&registry)
    {}

    void requireValid() const
    {
        if (!isValid()) {
            throw std::logic_error("The Entity is not valid.");
        }
    }

    template <typename Component>
    void requireComponent() const
    {
        requireValid();
        if (!m_registry->all_of<Component>(m_handle)) {
            throw std::out_of_range("The Entity does not have the requested Component.");
        }
    }

    entt::entity m_handle{entt::null};
    entt::registry* m_registry{nullptr};
};

} // namespace arti::scene
