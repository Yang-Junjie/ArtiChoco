#pragma once
#include "texture_format.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace arti::renderer::detail {
class TextureAccess;
}

namespace arti::renderer {

struct TextureCubeFaces {
    std::span<const std::byte> positive_x;
    std::span<const std::byte> negative_x;
    std::span<const std::byte> positive_y;
    std::span<const std::byte> negative_y;
    std::span<const std::byte> positive_z;
    std::span<const std::byte> negative_z;

    std::array<std::span<const std::byte>, 6> ordered() const noexcept {
        return { positive_x, negative_x, positive_y, negative_y, positive_z, negative_z };
    }
};

struct TextureCubeMipData {
    uint32_t size{ 0 };
    TextureCubeFaces faces;
};

class TextureCube {
public:
    ~TextureCube();

    TextureCube(const TextureCube&) = delete;
    TextureCube& operator=(const TextureCube&) = delete;
    TextureCube(TextureCube&&) noexcept;
    TextureCube& operator=(TextureCube&&) noexcept;

    uint32_t size() const noexcept;
    uint32_t mipLevels() const noexcept;
    TextureFormat format() const noexcept;

private:
    friend class detail::TextureAccess;
    struct Impl;

    explicit TextureCube(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::renderer
