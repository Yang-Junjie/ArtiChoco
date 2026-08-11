#pragma once
#include "artichoco/renderer/vulkan/vulkan_resource_state.h"

#include <vulkan/vulkan.hpp>

namespace arti::test_app {

inline vk::ImageSubresourceRange colorSubresourceRange()
{
    vk::ImageSubresourceRange range{};
    range.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    return range;
}

inline vk::ImageSubresourceRange depthSubresourceRange()
{
    vk::ImageSubresourceRange range{};
    range.setAspectMask(vk::ImageAspectFlagBits::eDepth)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    return range;
}

inline renderer::vulkan::VulkanImageState undefinedImageState()
{
    return {
        vk::PipelineStageFlagBits2::eNone,
        vk::AccessFlagBits2::eNone,
        vk::ImageLayout::eUndefined,
    };
}

inline renderer::vulkan::VulkanImageState colorAttachmentWriteState()
{
    return {
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eColorAttachmentOptimal,
    };
}

inline renderer::vulkan::VulkanImageState depthAttachmentReadWriteState()
{
    return {
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::ImageLayout::eDepthAttachmentOptimal,
    };
}

inline renderer::vulkan::VulkanImageState computeStorageWriteState()
{
    return {
        vk::PipelineStageFlagBits2::eComputeShader,
        vk::AccessFlagBits2::eShaderStorageWrite,
        vk::ImageLayout::eGeneral,
    };
}

inline renderer::vulkan::VulkanImageState transferReadState()
{
    return {
        vk::PipelineStageFlagBits2::eCopy,
        vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eTransferSrcOptimal,
    };
}

inline renderer::vulkan::VulkanImageState transferWriteState()
{
    return {
        vk::PipelineStageFlagBits2::eCopy,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eTransferDstOptimal,
    };
}

inline renderer::vulkan::VulkanImageState fragmentSampledReadState()
{
    return {
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderSampledRead,
        vk::ImageLayout::eShaderReadOnlyOptimal,
    };
}

inline renderer::vulkan::VulkanImageState presentState()
{
    return {
        vk::PipelineStageFlagBits2::eNone,
        vk::AccessFlagBits2::eNone,
        vk::ImageLayout::ePresentSrcKHR,
    };
}

} // namespace arti::test_app
