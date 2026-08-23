#include "nvrhi_mipmap.h"

#include "artichoco/renderer/slang_compiler.h"
#include "nvrhi_shader_factory.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace arti::renderer::vulkan {
namespace {

constexpr std::string_view kMipmapShader = R"slang(
[[vk::binding(0, 0)]] Texture2D<float4> source_texture;
[[vk::binding(1, 0)]] SamplerState source_sampler;
[[vk::binding(2, 0)]] RWTexture2D<float4> output_texture;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    output_texture.GetDimensions(width, height);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const float2 uv = (float2(dispatch_thread_id.xy) + 0.5) / float2(width, height);
    output_texture[dispatch_thread_id.xy] = source_texture.SampleLevel(source_sampler, uv, 0.0);
}
)slang";

const arti::renderer::CompiledComputeProgram& mipmapProgram()
{
    static const arti::renderer::CompiledComputeProgram program =
            arti::renderer::SlangCompiler::compileComputeSource(
                    kMipmapShader, "artichoco_nvrhi_mipmap.slang");
    return program;
}

} // namespace

void uploadAndGenerateNvrhiTextureMipmaps(nvrhi::IDevice& device,
        nvrhi::TextureHandle texture, std::span<const std::byte> base_level,
        size_t row_pitch, nvrhi::Format source_view_format,
        nvrhi::Format storage_view_format, nvrhi::ResourceStates final_state)
{
    if (!texture || base_level.empty() || row_pitch == 0 ||
            source_view_format == nvrhi::Format::UNKNOWN ||
            storage_view_format == nvrhi::Format::UNKNOWN ||
            final_state == nvrhi::ResourceStates::Unknown) {
        throw std::invalid_argument("NVRHI mipmap generation requires a texture and base level.");
    }
    const nvrhi::TextureDesc& texture_desc = texture->getDesc();
    if (!texture_desc.isUAV || !texture_desc.isTypeless ||
            texture_desc.mipLevels <= 1 || texture_desc.arraySize != 1 ||
            texture_desc.dimension != nvrhi::TextureDimension::Texture2D) {
        throw std::invalid_argument(
                "NVRHI mipmap generation requires a typeless 2D UAV texture with multiple mip levels.");
    }

    const auto& program = mipmapProgram();
    const auto shader_set = createNvrhiComputeShaderSet(
            device, program, "ArtiChoco NVRHI mipmap compute");
    if (!shader_set.compute_shader || shader_set.binding_layouts.empty() ||
            !shader_set.binding_layouts.front()) {
        throw std::runtime_error("NVRHI failed to create the mipmap compute shader.");
    }

    nvrhi::SamplerDesc sampler_desc;
    sampler_desc.setAllFilters(true);
    nvrhi::SamplerHandle sampler = device.createSampler(sampler_desc);
    nvrhi::ComputePipelineDesc pipeline_desc;
    pipeline_desc.setComputeShader(shader_set.compute_shader);
    for (const nvrhi::BindingLayoutHandle& layout : shader_set.binding_layouts) {
        if (layout) {
            pipeline_desc.addBindingLayout(layout);
        }
    }
    nvrhi::ComputePipelineHandle pipeline = device.createComputePipeline(pipeline_desc);
    if (!sampler || !pipeline) {
        throw std::runtime_error("NVRHI failed to create the mipmap compute pipeline.");
    }

    nvrhi::CommandListHandle command_list = device.createCommandList();
    if (!command_list) {
        throw std::runtime_error("NVRHI failed to create the mipmap command list.");
    }
    command_list->open();
    command_list->writeTexture(texture, 0, 0, base_level.data(), row_pitch);

    uint32_t mip_width = texture_desc.width;
    uint32_t mip_height = texture_desc.height;
    for (uint32_t mip_level = 1; mip_level < texture_desc.mipLevels; ++mip_level) {
        const uint32_t dst_width = std::max(1u, mip_width / 2);
        const uint32_t dst_height = std::max(1u, mip_height / 2);
        const nvrhi::TextureSubresourceSet source_subresources{
                mip_level - 1, 1, 0, 1};
        const nvrhi::TextureSubresourceSet destination_subresources{
                mip_level, 1, 0, 1};
        NvrhiBindingResource source =
                NvrhiBindingResource::Texture("source_texture", *texture);
        source.format = source_view_format;
        source.subresources = source_subresources;
        source.dimension = nvrhi::TextureDimension::Texture2D;
        NvrhiBindingResource sampler_resource =
                NvrhiBindingResource::Sampler("source_sampler", *sampler);
        NvrhiBindingResource destination =
                NvrhiBindingResource::Texture("output_texture", *texture);
        destination.format = storage_view_format;
        destination.subresources = destination_subresources;
        destination.dimension = nvrhi::TextureDimension::Texture2D;
        const std::array resources = {source, sampler_resource, destination};
        nvrhi::BindingSetHandle binding_set = createNvrhiBindingSet(
                device, program.reflection, 0, *shader_set.binding_layouts.front(), resources);
        if (!binding_set) {
            throw std::runtime_error("NVRHI failed to create a mipmap binding set.");
        }

        command_list->setTextureState(texture, source_subresources,
                nvrhi::ResourceStates::ShaderResource);
        command_list->setTextureState(texture, destination_subresources,
                nvrhi::ResourceStates::UnorderedAccess);
        nvrhi::ComputeState compute_state;
        compute_state.setPipeline(pipeline).addBindingSet(binding_set);
        command_list->setComputeState(compute_state);
        command_list->dispatch((dst_width + 7) / 8, (dst_height + 7) / 8, 1);
        mip_width = dst_width;
        mip_height = dst_height;
    }
    command_list->setPermanentTextureState(texture, final_state);
    command_list->close();
    device.executeCommandList(command_list);
}

} // namespace arti::renderer::vulkan
