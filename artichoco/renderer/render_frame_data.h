#pragma once
#include "index_buffer.h"
#include "texture_2d.h"
#include "vertex_buffer.h"

#include <cstdint>

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace arti::renderer {

struct RenderDrawCommand {
    std::shared_ptr<VertexBuffer> vertex_buffer;
    std::shared_ptr<IndexBuffer> index_buffer;
    std::shared_ptr<Texture2D> base_color_texture;
    glm::mat4 model_matrix{1.0f};
};

struct RenderFrameData {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    std::vector<RenderDrawCommand> draws;
    float time{0.0f};
    uint32_t frame_index{0};
};

} // namespace arti::renderer
