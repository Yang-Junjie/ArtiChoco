#pragma once
#include "artichoco/core/uuid.h"

#include <compare>
#include <functional>
#include <string>

namespace arti::asset {

using AssetType = std::string;

template<typename T>
class AssetHandle {
public:
    constexpr AssetHandle() noexcept = default;
    explicit constexpr AssetHandle(core::UUID id) noexcept
            : m_id(id) {}

    constexpr core::UUID id() const noexcept { return m_id; }
    constexpr bool isValid() const noexcept { return m_id.isValid(); }

    constexpr auto operator<=>(const AssetHandle&) const noexcept = default;

private:
    core::UUID m_id{};
};

class Asset {
public:
    explicit Asset(core::UUID handle = {}) noexcept
            : m_handle(handle) {}
    virtual ~Asset() = default;

    Asset(const Asset&) = default;
    Asset& operator=(const Asset&) = default;
    Asset(Asset&&) noexcept = default;
    Asset& operator=(Asset&&) noexcept = default;

    core::UUID getHandle() const noexcept { return m_handle; }
    virtual AssetType getType() const = 0;

    bool operator==(const Asset& other) const noexcept { return m_handle == other.m_handle; }

private:
    core::UUID m_handle;
};
}

namespace std {
template<typename T>
struct hash<arti::asset::AssetHandle<T>> {
    size_t operator()(arti::asset::AssetHandle<T> handle) const noexcept {
        return hash<arti::core::UUID>{}(handle.id());
    }
};
}
