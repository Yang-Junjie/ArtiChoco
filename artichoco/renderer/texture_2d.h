#pragma once
#include "texture_format.h"

#include <cstdint>
#include <memory>

namespace arti::renderer::detail {
class TextureAccess;
}

namespace arti::renderer {

class Texture2D {
public:
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    Texture2D(Texture2D&&) noexcept;
    Texture2D& operator=(Texture2D&&) noexcept;

    uint32_t width() const noexcept;
    uint32_t height() const noexcept;
    TextureFormat format() const noexcept;

private:
    friend class detail::TextureAccess;
    struct Impl;

    explicit Texture2D(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::renderer
