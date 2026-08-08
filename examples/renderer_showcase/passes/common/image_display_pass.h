#pragma once
#include "passes/common/showcase_pass.h"

#include <filesystem>
#include <memory>

namespace arti::renderer_showcase {
class SampledImageSource;

class ImageDisplayPass final : public ShowcasePass {
public:
    ImageDisplayPass(SampledImageSource& source, std::filesystem::path shader_path);
    ~ImageDisplayPass() override;

    void prepare(renderer::vulkan::VulkanPassPrepareContext& context) override;
    void record(renderer::vulkan::VulkanPassContext& context) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace arti::renderer_showcase
