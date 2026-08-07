#pragma once
#include "artichoco/renderer/vulkan/vulkan_pass.h"
#include "mrt_mesh_pass.h"

#include <filesystem>
#include <memory>

namespace arti::test_app {

class MrtCompositePass final : public renderer::vulkan::VulkanPass {
public:
    MrtCompositePass(MrtMeshPass& source, const std::filesystem::path& shader_path);
    ~MrtCompositePass() override;

    void prepare(renderer::vulkan::VulkanPassPrepareContext& context) override;
    void record(renderer::vulkan::VulkanPassContext& context) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::test_app
