#pragma once
#include "artichoco/renderer/vulkan/vulkan_resource_state.h"

#include <vulkan/vulkan.hpp>

namespace arti::renderer_showcase::pass_image_states {

inline vk::ImageSubresourceRange colorRange()
{
    vk::ImageSubresourceRange range;
    range.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    return range;
}

inline renderer::vulkan::VulkanImageState undefined()
{
    return {vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone, vk::ImageLayout::eUndefined};
}

inline renderer::vulkan::VulkanImageState colorAttachmentWrite()
{
    return {vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eColorAttachmentOptimal};
}

inline renderer::vulkan::VulkanImageState computeStorageWrite()
{
    return {vk::PipelineStageFlagBits2::eComputeShader,
            vk::AccessFlagBits2::eShaderStorageWrite,
            vk::ImageLayout::eGeneral};
}

inline renderer::vulkan::VulkanImageState fragmentSampledRead()
{
    return {vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderSampledRead,
            vk::ImageLayout::eShaderReadOnlyOptimal};
}

inline renderer::vulkan::VulkanImageState present()
{
    return {vk::PipelineStageFlagBits2::eNone,
            vk::AccessFlagBits2::eNone,
            vk::ImageLayout::ePresentSrcKHR};
}

} // namespace arti::renderer_showcase::pass_image_states
