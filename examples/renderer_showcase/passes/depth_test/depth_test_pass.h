#pragma once
#include "passes/common/indexed_graphics_pass.h"

namespace arti::renderer {
class IndexBuffer;
class VertexBuffer;
} // namespace arti::renderer

namespace arti::renderer_showcase {

class DepthTestPass final : public IndexedGraphicsPass {
public:
    DepthTestPass(renderer::VertexBuffer vertex_buffer,
                  renderer::IndexBuffer index_buffer,
                  std::filesystem::path shader_path);
    ~DepthTestPass() override;
};

} // namespace arti::renderer_showcase
