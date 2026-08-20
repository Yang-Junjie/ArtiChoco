#pragma once

#include "artichoco/renderer/vertex_buffer.h"
#include "artichoco/renderer/vulkan/vulkan_pass.h"

#include <filesystem>
#include <memory>

namespace arti::renderer::vulkan {
class VulkanBindingLayout;
class VulkanShader;
} // namespace arti::renderer::vulkan

namespace arti::hello_triangle {

class TrianglePass final : public renderer::vulkan::VulkanPass {
public:
    TrianglePass(renderer::VertexBuffer vertex_buffer, std::filesystem::path shader_path);
    ~TrianglePass() override;

    TrianglePass(const TrianglePass&) = delete;
    TrianglePass& operator=(const TrianglePass&) = delete;

    void prepare(renderer::vulkan::VulkanPassPrepareContext& context) override;
    void record(renderer::vulkan::VulkanPassContext& context) override;

private:
    renderer::VertexBuffer m_vertex_buffer;
    std::filesystem::path m_shader_path;
    std::unique_ptr<renderer::vulkan::VulkanShader> m_shader;
    std::unique_ptr<renderer::vulkan::VulkanBindingLayout> m_binding_layout;
};

} // namespace arti::hello_triangle
