#pragma once

#include <cstdint>
#include <memory>

namespace arti::renderer::detail {
class BufferAccess;
}

namespace arti::renderer {

enum class IndexType {
    UInt16,
    UInt32,
};

class IndexBuffer {
public:
    ~IndexBuffer();

    IndexBuffer(const IndexBuffer&) = delete;
    IndexBuffer& operator=(const IndexBuffer&) = delete;
    IndexBuffer(IndexBuffer&&) noexcept;
    IndexBuffer& operator=(IndexBuffer&&) noexcept;

    uint32_t indexCount() const noexcept;
    IndexType indexType() const noexcept;

private:
    friend class detail::BufferAccess;
    struct Impl;

    explicit IndexBuffer(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::renderer
