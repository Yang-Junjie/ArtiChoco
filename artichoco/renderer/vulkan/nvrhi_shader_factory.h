#pragma once

#include "artichoco/renderer/slang_compiler.h"

#include <nvrhi/nvrhi.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arti::renderer::vulkan {

struct NvrhiGraphicsShaderSet {
    nvrhi::ShaderHandle vertex_shader;
    nvrhi::ShaderHandle pixel_shader;
    std::vector<nvrhi::BindingLayoutHandle> binding_layouts;
};

struct NvrhiComputeShaderSet {
    nvrhi::ShaderHandle compute_shader;
    std::vector<nvrhi::BindingLayoutHandle> binding_layouts;
};

enum class NvrhiBindingResourceKind {
    Texture,
    Buffer,
    Sampler,
};

struct NvrhiBindingResource {
    std::string name;
    NvrhiBindingResourceKind kind{NvrhiBindingResourceKind::Texture};
    uint32_t array_element{0};
    nvrhi::ITexture* texture{nullptr};
    nvrhi::IBuffer* buffer{nullptr};
    nvrhi::ISampler* sampler{nullptr};
    nvrhi::Format format{nvrhi::Format::UNKNOWN};
    nvrhi::TextureSubresourceSet subresources{nvrhi::AllSubresources};
    nvrhi::TextureDimension dimension{nvrhi::TextureDimension::Unknown};
    nvrhi::BufferRange range{nvrhi::EntireBuffer};

    static NvrhiBindingResource Texture(std::string name, nvrhi::ITexture& texture,
            uint32_t array_element = 0);
    static NvrhiBindingResource Buffer(std::string name, nvrhi::IBuffer& buffer,
            uint32_t array_element = 0);
    static NvrhiBindingResource Sampler(std::string name, nvrhi::ISampler& sampler,
            uint32_t array_element = 0);
};

NvrhiGraphicsShaderSet createNvrhiGraphicsShaderSet(nvrhi::IDevice& device,
        const CompiledGraphicsProgram& program, std::string_view debug_name = {},
        uint32_t bindless_capacity = 0);

NvrhiComputeShaderSet createNvrhiComputeShaderSet(nvrhi::IDevice& device,
        const CompiledComputeProgram& program, std::string_view debug_name = {},
        uint32_t bindless_capacity = 0);

nvrhi::BindingSetHandle createNvrhiBindingSet(nvrhi::IDevice& device,
        const ShaderReflection& reflection, uint32_t descriptor_set,
        nvrhi::IBindingLayout& layout, std::span<const NvrhiBindingResource> resources);

} // namespace arti::renderer::vulkan
