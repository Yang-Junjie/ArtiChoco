#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace arti::renderer::detail {
class BufferAccess;
}

namespace arti::renderer {

enum class VertexAttributeType {
    Float2,
    Float3,
    Float4,
};

struct VertexAttribute {
    uint32_t location{0};
    VertexAttributeType type{VertexAttributeType::Float3};
    uint32_t offset{0};

    bool operator==(const VertexAttribute&) const = default;
};

struct VertexBufferLayout {
    uint32_t stride{0};
    std::vector<VertexAttribute> attributes;

    bool operator==(const VertexBufferLayout&) const = default;
};

class VertexBuffer {
public:
    ~VertexBuffer();

    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;
    VertexBuffer(VertexBuffer&&) noexcept;
    VertexBuffer& operator=(VertexBuffer&&) noexcept;

    uint32_t vertexCount() const noexcept;
    const VertexBufferLayout& layout() const noexcept;

private:
    friend class detail::BufferAccess;
    struct Impl;

    explicit VertexBuffer(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::renderer
