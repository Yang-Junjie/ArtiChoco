#pragma once
#include "passes/common/indexed_graphics_pass.h"

namespace arti::renderer {
class IndexBuffer;
class VertexBuffer;
} // namespace arti::renderer

namespace arti::renderer_showcase {

class TrianglePass final : public IndexedGraphicsPass {
public:
    TrianglePass(renderer::VertexBuffer vertex_buffer,
                 renderer::IndexBuffer index_buffer,
                 std::filesystem::path shader_path);
    ~TrianglePass() override;
};

} // namespace arti::renderer_showcase
