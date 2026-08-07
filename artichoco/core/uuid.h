#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace arti::core {

class UUID {
public:
    using Value = uint64_t;

    constexpr UUID() noexcept = default;
    explicit constexpr UUID(Value value) noexcept
        : m_value(value)
    {}

    [[nodiscard]] static UUID generate();
    [[nodiscard]] static std::optional<UUID> fromString(std::string_view value) noexcept;

    [[nodiscard]] constexpr Value value() const noexcept
    {
        return m_value;
    }

    explicit constexpr operator Value() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] std::string toString() const;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_value != 0;
    }

    auto operator<=>(const UUID&) const noexcept = default;

private:
    Value m_value{0};
};

} // namespace arti::core

namespace std {

template <>
struct hash<arti::core::UUID> {
    size_t operator()(arti::core::UUID uuid) const noexcept
    {
        return hash<arti::core::UUID::Value>{}(uuid.value());
    }
};

} // namespace std
