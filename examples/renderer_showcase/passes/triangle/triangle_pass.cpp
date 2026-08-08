#include "passes/triangle/triangle_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/vertex_buffer.h"

#include <utility>

namespace arti::renderer_showcase {

TrianglePass::TrianglePass(renderer::VertexBuffer vertex_buffer,
                           renderer::IndexBuffer index_buffer,
                           std::filesystem::path shader_path)
    : IndexedGraphicsPass(std::move(vertex_buffer),
                          std::move(index_buffer),
                          std::move(shader_path),
                          {0.025f, 0.035f, 0.065f, 1.0f})
{}

TrianglePass::~TrianglePass() = default;

} // namespace arti::renderer_showcase
