#pragma once

#include <cstdint>

#include <filesystem>
#include <string>
#include <vector>

namespace arti::renderer {

struct CompiledShaderStage {
    std::string entry_point;
    std::vector<uint32_t> spirv;
};

enum class ShaderResourceType {
    Sampler,
    SampledImage,
    StorageImage,
    UniformBuffer,
    StorageBuffer,
    UniformTexelBuffer,
    StorageTexelBuffer,
    CombinedImageSampler,
};

enum class ShaderStageMask : uint32_t {
    None = 0,
    Vertex = 1 << 0,
    Fragment = 1 << 1,
    Compute = 1 << 2,
};

constexpr ShaderStageMask operator|(ShaderStageMask lhs, ShaderStageMask rhs) noexcept
{
    return static_cast<ShaderStageMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

struct ReflectedShaderBinding {
    std::string name;
    ShaderResourceType type{ShaderResourceType::SampledImage};
    uint32_t set{0};
    uint32_t binding{0};
    uint32_t count{1};
    ShaderStageMask stages{ShaderStageMask::None};
};

struct ReflectedPushConstantRange {
    uint32_t offset{0};
    uint32_t size{0};
    ShaderStageMask stages{ShaderStageMask::None};
};

struct ShaderReflection {
    std::vector<ReflectedShaderBinding> bindings;
    std::vector<ReflectedPushConstantRange> push_constants;
};

struct CompiledGraphicsProgram {
    CompiledShaderStage vertex;
    CompiledShaderStage fragment;
    ShaderReflection reflection;
};

struct CompiledComputeProgram {
    CompiledShaderStage compute;
    ShaderReflection reflection;
    uint32_t thread_group_size_x{1};
    uint32_t thread_group_size_y{1};
    uint32_t thread_group_size_z{1};
};

class SlangCompiler {
public:
    static CompiledGraphicsProgram compileGraphics(const std::filesystem::path& source_path,
                                                   std::string vertex_entry_point,
                                                   std::string fragment_entry_point);
    static CompiledComputeProgram compileCompute(const std::filesystem::path& source_path,
                                                 std::string compute_entry_point);
};

} // namespace arti::renderer
