#include "nvrhi_shader_factory.h"

#include <algorithm>
#include <limits>
#include <map>
#include <ranges>
#include <stdexcept>
#include <string>

namespace arti::renderer::vulkan {
namespace {

nvrhi::ShaderType toNvrhiShaderType(ShaderStageMask stages)
{
    nvrhi::ShaderType result = nvrhi::ShaderType::None;
    const auto mask = static_cast<uint32_t>(stages);
    if (mask & static_cast<uint32_t>(ShaderStageMask::Vertex)) {
        result = result | nvrhi::ShaderType::Vertex;
    }
    if (mask & static_cast<uint32_t>(ShaderStageMask::Fragment)) {
        result = result | nvrhi::ShaderType::Pixel;
    }
    if (mask & static_cast<uint32_t>(ShaderStageMask::Compute)) {
        result = result | nvrhi::ShaderType::Compute;
    }
    if (result == nvrhi::ShaderType::None) {
        throw std::invalid_argument("A reflected NVRHI binding has no visible shader stage.");
    }
    return result;
}

nvrhi::BindingLayoutItem toNvrhiBindingItem(const ReflectedShaderBinding& binding)
{
    if (binding.count == 0 || binding.count > std::numeric_limits<uint16_t>::max()) {
        throw std::invalid_argument("NVRHI binding array count is outside the supported range.");
    }

    nvrhi::BindingLayoutItem result;
    switch (binding.type) {
        case ShaderResourceType::Sampler:
            result = nvrhi::BindingLayoutItem::Sampler(binding.binding);
            break;
        case ShaderResourceType::SampledImage:
            result = nvrhi::BindingLayoutItem::Texture_SRV(binding.binding);
            break;
        case ShaderResourceType::StorageImage:
            result = nvrhi::BindingLayoutItem::Texture_UAV(binding.binding);
            break;
        case ShaderResourceType::UniformBuffer:
            result = nvrhi::BindingLayoutItem::ConstantBuffer(binding.binding);
            break;
        case ShaderResourceType::StorageBuffer:
        case ShaderResourceType::StorageBufferReadOnly:
            result = nvrhi::BindingLayoutItem::RawBuffer_SRV(binding.binding);
            break;
        case ShaderResourceType::StorageBufferReadWrite:
            result = nvrhi::BindingLayoutItem::RawBuffer_UAV(binding.binding);
            break;
        case ShaderResourceType::StructuredBufferReadOnly:
            result = nvrhi::BindingLayoutItem::StructuredBuffer_SRV(binding.binding);
            break;
        case ShaderResourceType::StructuredBufferReadWrite:
            result = nvrhi::BindingLayoutItem::StructuredBuffer_UAV(binding.binding);
            break;
        case ShaderResourceType::UniformTexelBuffer:
            result = nvrhi::BindingLayoutItem::TypedBuffer_SRV(binding.binding);
            break;
        case ShaderResourceType::StorageTexelBuffer:
            result = nvrhi::BindingLayoutItem::TypedBuffer_UAV(binding.binding);
            break;
        case ShaderResourceType::CombinedImageSampler:
            throw std::invalid_argument(
                    "Combined image samplers are not supported by the NVRHI binding bridge; use separate texture and sampler bindings.");
    }
    result.setSize(binding.count);
    return result;
}

struct LayoutBuildState {
    nvrhi::BindingLayoutDesc desc;
    std::map<uint32_t, nvrhi::ResourceType> slots;
};

nvrhi::VulkanBindingOffsets zeroVulkanBindingOffsets()
{
    nvrhi::VulkanBindingOffsets offsets;
    offsets.setShaderResourceOffset(0)
            .setSamplerOffset(0)
            .setConstantBufferOffset(0)
            .setUnorderedAccessViewOffset(0);
    return offsets;
}

std::vector<nvrhi::BindingLayoutHandle> createBindingLayouts(
        nvrhi::IDevice& device, const ShaderReflection& reflection, uint32_t bindless_capacity)
{
    std::map<uint32_t, LayoutBuildState> states;
    std::map<uint32_t, ReflectedShaderBinding> unbounded_bindings;
    for (const auto& binding : reflection.bindings) {
        if (binding.unbounded) {
            if (!unbounded_bindings.emplace(binding.set, binding).second) {
                throw std::invalid_argument(
                        "A bindless descriptor set may contain only one unbounded binding.");
            }
            continue;
        }
        auto& state = states[binding.set];
        if (state.desc.visibility == nvrhi::ShaderType::None) {
            state.desc.setVisibility(toNvrhiShaderType(binding.stages));
            state.desc.setRegisterSpaceAndDescriptorSet(binding.set);
            state.desc.bindingOffsets = zeroVulkanBindingOffsets();
        } else {
            state.desc.visibility = state.desc.visibility | toNvrhiShaderType(binding.stages);
        }

        const nvrhi::BindingLayoutItem item = toNvrhiBindingItem(binding);
        const auto [slot, inserted] = state.slots.emplace(item.slot, item.type);
        if (!inserted) {
            throw std::invalid_argument("Reflected NVRHI bindings reuse a descriptor slot.");
        }
        state.desc.addItem(item);
    }

    for (const auto& push_constant : reflection.push_constants) {
        if (push_constant.size == 0 || push_constant.size > std::numeric_limits<uint16_t>::max()) {
            throw std::invalid_argument("Reflected NVRHI push constants exceed the supported size.");
        }
        auto& state = states[0];
        if (state.desc.visibility == nvrhi::ShaderType::None) {
            state.desc.setRegisterSpaceAndDescriptorSet(0);
            state.desc.bindingOffsets = zeroVulkanBindingOffsets();
        }
        state.desc.visibility = state.desc.visibility | toNvrhiShaderType(push_constant.stages);
        const auto existing = std::ranges::find_if(state.desc.bindings, [](const auto& item) {
            return item.type == nvrhi::ResourceType::PushConstants;
        });
        if (existing == state.desc.bindings.end()) {
            state.desc.addItem(nvrhi::BindingLayoutItem::PushConstants(0, push_constant.size));
        } else if (existing->size != push_constant.size) {
            throw std::invalid_argument("Reflected push constant ranges do not have one consistent size.");
        }
    }

    if (states.empty()) {
        if (unbounded_bindings.empty()) {
            return {};
        }
    }

    const uint32_t maximum_set = std::max(
            states.empty() ? 0u : states.rbegin()->first,
            unbounded_bindings.empty() ? 0u : unbounded_bindings.rbegin()->first);
    std::vector<nvrhi::BindingLayoutHandle> layouts(static_cast<size_t>(maximum_set) + 1);
    for (auto& [set, state] : states) {
        if (unbounded_bindings.contains(set)) {
            throw std::invalid_argument(
                    "An unbounded binding must occupy its own descriptor set.");
        }
        layouts[set] = device.createBindingLayout(state.desc);
        if (layouts[set].Get() == nullptr) {
            throw std::runtime_error("NVRHI failed to create a reflected binding layout.");
        }
    }
    for (const auto& [set, binding] : unbounded_bindings) {
        if (binding.type != ShaderResourceType::SampledImage || binding.binding != 0) {
            throw std::invalid_argument(
                    "NVRHI bindless reflection requires one sampled-image binding at slot zero.");
        }
        if (bindless_capacity == 0) {
            throw std::invalid_argument(
                    "A non-zero bindless capacity is required for an unbounded texture array.");
        }
        nvrhi::BindlessLayoutDesc bindless_desc;
        bindless_desc.setVisibility(toNvrhiShaderType(binding.stages))
                .setFirstSlot(0)
                .setMaxCapacity(bindless_capacity)
                .setLayoutType(nvrhi::BindlessLayoutDesc::LayoutType::Immutable)
                .addRegisterSpace(nvrhi::BindingLayoutItem::Texture_SRV(0));
        layouts[set] = device.createBindlessLayout(bindless_desc);
        if (layouts[set].Get() == nullptr) {
            throw std::runtime_error("NVRHI failed to create a bindless layout.");
        }
    }
    return layouts;
}

nvrhi::ShaderHandle createShader(nvrhi::IDevice& device, const CompiledShaderStage& stage,
        nvrhi::ShaderType type, std::string_view debug_name)
{
    if (stage.spirv.empty()) {
        throw std::invalid_argument("Cannot create an NVRHI shader from empty SPIR-V.");
    }
    nvrhi::ShaderDesc desc;
    desc.setShaderType(type).setEntryName(stage.entry_point);
    if (!debug_name.empty()) {
        desc.setDebugName(std::string{debug_name});
    }
    auto shader = device.createShader(desc, stage.spirv.data(), stage.spirv.size() * sizeof(uint32_t));
    if (shader.Get() == nullptr) {
        throw std::runtime_error("NVRHI failed to create a shader from Slang SPIR-V.");
    }
    return shader;
}

nvrhi::BindingSetItem toNvrhiBindingSetItem(
        const ReflectedShaderBinding& binding, const NvrhiBindingResource& resource)
{
    nvrhi::BindingSetItem result;
    switch (binding.type) {
        case ShaderResourceType::Sampler:
            if (resource.kind != NvrhiBindingResourceKind::Sampler || resource.sampler == nullptr) {
                throw std::invalid_argument("A reflected sampler requires an NVRHI sampler resource.");
            }
            result = nvrhi::BindingSetItem::Sampler(binding.binding, resource.sampler);
            break;
        case ShaderResourceType::SampledImage:
            if (resource.kind != NvrhiBindingResourceKind::Texture || resource.texture == nullptr) {
                throw std::invalid_argument("A reflected sampled image requires an NVRHI texture resource.");
            }
            result = nvrhi::BindingSetItem::Texture_SRV(binding.binding, resource.texture,
                    resource.format, resource.subresources, resource.dimension);
            break;
        case ShaderResourceType::StorageImage:
            if (resource.kind != NvrhiBindingResourceKind::Texture || resource.texture == nullptr) {
                throw std::invalid_argument("A reflected storage image requires an NVRHI texture resource.");
            }
            result = nvrhi::BindingSetItem::Texture_UAV(binding.binding, resource.texture,
                    resource.format, resource.subresources, resource.dimension);
            break;
        case ShaderResourceType::UniformBuffer:
            if (resource.kind != NvrhiBindingResourceKind::Buffer || resource.buffer == nullptr) {
                throw std::invalid_argument("A reflected constant buffer requires an NVRHI buffer resource.");
            }
            result = nvrhi::BindingSetItem::ConstantBuffer(
                    binding.binding, resource.buffer, resource.range);
            break;
        case ShaderResourceType::StorageBuffer:
        case ShaderResourceType::StorageBufferReadOnly:
            if (resource.kind != NvrhiBindingResourceKind::Buffer || resource.buffer == nullptr) {
                throw std::invalid_argument("A reflected buffer SRV requires an NVRHI buffer resource.");
            }
            result = nvrhi::BindingSetItem::RawBuffer_SRV(
                    binding.binding, resource.buffer, resource.range);
            break;
        case ShaderResourceType::StorageBufferReadWrite:
            if (resource.kind != NvrhiBindingResourceKind::Buffer || resource.buffer == nullptr) {
                throw std::invalid_argument("A reflected buffer UAV requires an NVRHI buffer resource.");
            }
            result = nvrhi::BindingSetItem::RawBuffer_UAV(
                    binding.binding, resource.buffer, resource.range);
            break;
        case ShaderResourceType::StructuredBufferReadOnly:
            if (resource.kind != NvrhiBindingResourceKind::Buffer || resource.buffer == nullptr) {
                throw std::invalid_argument("A reflected structured buffer SRV requires an NVRHI buffer resource.");
            }
            result = nvrhi::BindingSetItem::StructuredBuffer_SRV(
                    binding.binding, resource.buffer, resource.format, resource.range);
            break;
        case ShaderResourceType::StructuredBufferReadWrite:
            if (resource.kind != NvrhiBindingResourceKind::Buffer || resource.buffer == nullptr) {
                throw std::invalid_argument("A reflected structured buffer UAV requires an NVRHI buffer resource.");
            }
            result = nvrhi::BindingSetItem::StructuredBuffer_UAV(
                    binding.binding, resource.buffer, resource.format, resource.range);
            break;
        case ShaderResourceType::UniformTexelBuffer:
            if (resource.kind != NvrhiBindingResourceKind::Buffer || resource.buffer == nullptr) {
                throw std::invalid_argument("A reflected typed buffer SRV requires an NVRHI buffer resource.");
            }
            result = nvrhi::BindingSetItem::TypedBuffer_SRV(
                    binding.binding, resource.buffer, resource.format, resource.range);
            break;
        case ShaderResourceType::StorageTexelBuffer:
            if (resource.kind != NvrhiBindingResourceKind::Buffer || resource.buffer == nullptr) {
                throw std::invalid_argument("A reflected typed buffer UAV requires an NVRHI buffer resource.");
            }
            result = nvrhi::BindingSetItem::TypedBuffer_UAV(
                    binding.binding, resource.buffer, resource.format, resource.range);
            break;
        case ShaderResourceType::CombinedImageSampler:
            throw std::invalid_argument("Combined image samplers are not supported by NVRHI binding sets.");
    }
    result.setArrayElement(resource.array_element);
    return result;
}

} // namespace

NvrhiBindingResource NvrhiBindingResource::Texture(
        std::string name, nvrhi::ITexture& texture, uint32_t array_element)
{
    NvrhiBindingResource result;
    result.name = std::move(name);
    result.kind = NvrhiBindingResourceKind::Texture;
    result.array_element = array_element;
    result.texture = &texture;
    return result;
}

NvrhiBindingResource NvrhiBindingResource::Buffer(
        std::string name, nvrhi::IBuffer& buffer, uint32_t array_element)
{
    NvrhiBindingResource result;
    result.name = std::move(name);
    result.kind = NvrhiBindingResourceKind::Buffer;
    result.array_element = array_element;
    result.buffer = &buffer;
    return result;
}

NvrhiBindingResource NvrhiBindingResource::Sampler(
        std::string name, nvrhi::ISampler& sampler, uint32_t array_element)
{
    NvrhiBindingResource result;
    result.name = std::move(name);
    result.kind = NvrhiBindingResourceKind::Sampler;
    result.array_element = array_element;
    result.sampler = &sampler;
    return result;
}

NvrhiGraphicsShaderSet createNvrhiGraphicsShaderSet(nvrhi::IDevice& device,
        const CompiledGraphicsProgram& program, std::string_view debug_name,
        uint32_t bindless_capacity)
{
    NvrhiGraphicsShaderSet result;
    result.vertex_shader = createShader(device, program.vertex, nvrhi::ShaderType::Vertex, debug_name);
    result.pixel_shader = createShader(device, program.fragment, nvrhi::ShaderType::Pixel, debug_name);
    result.binding_layouts = createBindingLayouts(device, program.reflection, bindless_capacity);
    return result;
}

NvrhiComputeShaderSet createNvrhiComputeShaderSet(nvrhi::IDevice& device,
        const CompiledComputeProgram& program, std::string_view debug_name,
        uint32_t bindless_capacity)
{
    NvrhiComputeShaderSet result;
    result.compute_shader = createShader(device, program.compute, nvrhi::ShaderType::Compute, debug_name);
    result.binding_layouts = createBindingLayouts(device, program.reflection, bindless_capacity);
    return result;
}

nvrhi::BindingSetHandle createNvrhiBindingSet(nvrhi::IDevice& device,
        const ShaderReflection& reflection, uint32_t descriptor_set,
        nvrhi::IBindingLayout& layout, std::span<const NvrhiBindingResource> resources)
{
    std::vector<bool> resource_used(resources.size(), false);
    nvrhi::BindingSetDesc desc;

    for (const ReflectedShaderBinding& binding : reflection.bindings) {
        if (binding.set != descriptor_set) {
            continue;
        }
        if (binding.unbounded) {
            throw std::invalid_argument(
                    "Unbounded bindings require an NVRHI descriptor table, not a binding set.");
        }
        for (uint32_t array_element = 0; array_element < binding.count; ++array_element) {
            size_t match = resources.size();
            for (size_t index = 0; index < resources.size(); ++index) {
                if (resources[index].name == binding.name &&
                        resources[index].array_element == array_element) {
                    if (match != resources.size()) {
                        throw std::invalid_argument(
                                "An NVRHI binding resource name and array element are duplicated.");
                    }
                    match = index;
                }
            }
            if (match == resources.size()) {
                throw std::invalid_argument(
                        "A reflected NVRHI binding has no matching resource: " + binding.name);
            }
            resource_used[match] = true;
            desc.addItem(toNvrhiBindingSetItem(binding, resources[match]));
        }
    }

    if (descriptor_set == 0) {
        for (const ReflectedPushConstantRange& push_constant : reflection.push_constants) {
            const auto existing = std::ranges::find_if(desc.bindings, [](const auto& item) {
                return item.type == nvrhi::ResourceType::PushConstants;
            });
            if (existing == desc.bindings.end()) {
                desc.addItem(nvrhi::BindingSetItem::PushConstants(0, push_constant.size));
            } else if (existing->range.byteSize != push_constant.size) {
                throw std::invalid_argument(
                        "Reflected push constant ranges do not have one consistent size.");
            }
        }
    }

    if (std::ranges::find(resource_used, false) != resource_used.end()) {
        throw std::invalid_argument(
                "An NVRHI binding resource is not declared by the requested descriptor set.");
    }

    nvrhi::BindingSetHandle binding_set = device.createBindingSet(desc, &layout);
    if (!binding_set) {
        throw std::runtime_error("NVRHI failed to create a reflected binding set.");
    }
    return binding_set;
}

} // namespace arti::renderer::vulkan
