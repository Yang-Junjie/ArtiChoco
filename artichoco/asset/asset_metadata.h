#pragma once

#include "asset.h"

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace arti::asset {

class AssetMetadataValue {
public:
    using Storage = std::variant<bool, int64_t, uint64_t, double, std::string>;

    AssetMetadataValue() = default;
    AssetMetadataValue(bool value)
            : m_value(value) {}

    template<std::signed_integral Integer>
        requires(!std::same_as<std::remove_cv_t<Integer>, bool>)
    AssetMetadataValue(Integer value)
            : m_value(static_cast<int64_t>(value)) {}

    template<std::unsigned_integral Integer>
        requires(!std::same_as<std::remove_cv_t<Integer>, bool>)
    AssetMetadataValue(Integer value)
            : m_value(static_cast<uint64_t>(value)) {}

    template<std::floating_point Number>
    AssetMetadataValue(Number value)
            : m_value(static_cast<double>(value)) {}

    AssetMetadataValue(const char* value)
            : m_value(std::string{ value }) {}
    AssetMetadataValue(std::string_view value)
            : m_value(std::string{ value }) {}
    AssetMetadataValue(std::string value)
            : m_value(std::move(value)) {}

    template<typename T>
    [[nodiscard]] bool is() const noexcept {
        return std::holds_alternative<T>(m_value);
    }

    template<typename T>
    [[nodiscard]] const T& get() const {
        return std::get<T>(m_value);
    }

    template<typename T>
    [[nodiscard]] const T* getIf() const noexcept {
        return std::get_if<T>(&m_value);
    }

    [[nodiscard]] const Storage& data() const noexcept { return m_value; }

    bool operator==(const AssetMetadataValue&) const = default;

private:
    Storage m_value{ false };
};

using AssetMetadataProperties = std::map<std::string, AssetMetadataValue, std::less<>>;

struct AssetMetadata {
    core::UUID handle;
    AssetType type;
    std::filesystem::path source_path;
    std::filesystem::path artifact_path;
    std::vector<core::UUID> dependencies;
    AssetMetadataProperties properties;

    bool operator==(const AssetMetadata&) const = default;
};

[[nodiscard]] bool isSafeAssetRelativePath(const std::filesystem::path& path,
        bool allow_empty = false);
[[nodiscard]] bool isValidAssetMetadata(const AssetMetadata& metadata);

} // namespace arti::asset
