#pragma once
#include <cstdint>

#include <functional>
#include <string>

namespace arti::core {
class UUID {
public:
    UUID();
    explicit UUID(uint64_t uuid);
    UUID(const UUID&) = default;

    explicit operator uint64_t() const
    {
        return m_uuid;
    }

    bool operator==(const UUID& other) const
    {
        return m_uuid == other.m_uuid;
    }

    bool operator!=(const UUID& other) const
    {
        return m_uuid != other.m_uuid;
    }

    std::string toString() const
    {
        return std::to_string(m_uuid);
    }

    bool isValid() const
    {
        return m_uuid != 0;
    }

private:
    uint64_t m_uuid;
};
} // namespace arti::core

namespace std {
template <> struct hash<arti::core::UUID> {
    size_t operator()(const arti::core::UUID& uuid) const noexcept
    {
        return hash<uint64_t>{}(static_cast<uint64_t>(uuid));
    }
};
} // namespace std
