#pragma once
#include "artichoco/renderer/index_buffer.h"
#include "artichoco/renderer/texture_2d.h"
#include "artichoco/renderer/vertex_buffer.h"

#include <cstdint>

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace arti::test_app {

struct RenderDrawCommand {
    std::shared_ptr<renderer::VertexBuffer> vertex_buffer;
    std::shared_ptr<renderer::IndexBuffer> index_buffer;
    std::shared_ptr<renderer::Texture2D> base_color_texture;
    glm::mat4 model_matrix{1.0f};
};

struct RenderFrameData {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    std::vector<RenderDrawCommand> draws;
    float time{0.0f};
    uint32_t frame_index{0};
};

} // namespace arti::test_app
