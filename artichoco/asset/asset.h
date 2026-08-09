#pragma once

#include "artichoco/core/uuid.h"

#include <concepts>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>

namespace arti::asset {

class AssetType {
public:
    AssetType() = default;
    explicit AssetType(std::string_view name)
            : m_name(name) {}

    [[nodiscard]] bool isValid() const noexcept { return !m_name.empty(); }
    explicit operator bool() const noexcept { return isValid(); }

    [[nodiscard]] std::string_view name() const noexcept { return m_name; }

    bool operator==(const AssetType&) const noexcept = default;

private:
    std::string m_name;
};

template<typename T>
struct AssetTraits;

template<typename T>
concept HasAssetTraits = requires {
    { AssetTraits<std::remove_cvref_t<T>>::name } -> std::convertible_to<std::string_view>;
};

template<HasAssetTraits T>
[[nodiscard]] AssetType assetType() {
    return AssetType{ std::string_view{ AssetTraits<std::remove_cvref_t<T>>::name } };
}

class Asset {
public:
    explicit Asset(core::UUID handle = {}) noexcept
            : m_handle(handle) {}
    virtual ~Asset() = default;

    Asset(const Asset&) = default;
    Asset& operator=(const Asset&) = default;
    Asset(Asset&&) noexcept = default;
    Asset& operator=(Asset&&) noexcept = default;

    [[nodiscard]] core::UUID getHandle() const noexcept { return m_handle; }
    [[nodiscard]] virtual AssetType getType() const = 0;

    bool operator==(const Asset& other) const noexcept { return m_handle == other.m_handle; }

private:
    core::UUID m_handle;
};

} // namespace arti::asset

namespace std {

template<>
struct hash<arti::asset::AssetType> {
    size_t operator()(const arti::asset::AssetType& type) const noexcept {
        return hash<string_view>{}(type.name());
    }
};

} // namespace std
