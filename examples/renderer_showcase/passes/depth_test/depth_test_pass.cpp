#include "passes/depth_test/depth_test_pass.h"

#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/vertex_buffer.h"

#include <utility>

namespace arti::renderer_showcase {

DepthTestPass::DepthTestPass(renderer::VertexBuffer vertex_buffer,
                             renderer::IndexBuffer index_buffer,
                             std::filesystem::path shader_path)
    : IndexedGraphicsPass(std::move(vertex_buffer),
                          std::move(index_buffer),
                          std::move(shader_path),
                          {0.02f, 0.025f, 0.04f, 1.0f},
                          true)
{}

DepthTestPass::~DepthTestPass() = default;

} // namespace arti::renderer_showcase
