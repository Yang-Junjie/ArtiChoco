#include "throw_once_pass.h"

#include "artichoco/renderer/vulkan/vulkan_frame_manager.h"
#include "artichoco/renderer/vulkan/vulkan_pass_context.h"
#include "render_pass_common.h"

#include <stdexcept>

namespace arti::test_app {

void ThrowOncePass::record(renderer::vulkan::VulkanPassContext& context)
{
    if (m_did_throw) {
        return;
    }

    auto& frame = context.frame();
    const auto to_color_attachment = renderer::vulkan::makeImageBarrier(
        frame.colorImage(), colorSubresourceRange(), undefinedImageState(), colorAttachmentWriteState());

    auto& commands = context.commands();
    commands.imageBarrier(to_color_attachment);
    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.setImageView(frame.colorImageView())
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore);
    vk::RenderingInfo rendering_info{};
    rendering_info.setRenderArea(vk::Rect2D{{0, 0}, frame.extent()})
        .setLayerCount(1)
        .setColorAttachments(color_attachment);
    commands.beginRendering(rendering_info);

    m_did_throw = true;
    throw std::runtime_error("Intentional Vulkan frame recording failure.");
}

bool ThrowOncePass::didThrow() const noexcept
{
    return m_did_throw;
}

} // namespace arti::test_app
