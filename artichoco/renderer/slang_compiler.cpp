#include "slang_compiler.h"
#include "renderer_log.h"

#include <cstring>

#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <slang/slang-com-ptr.h>
#include <slang/slang.h>
#include <stdexcept>
#include <string_view>

namespace arti::renderer {
namespace {

void reportDiagnostics(slang::IBlob* diagnostics, bool failed) {
    if (diagnostics == nullptr || diagnostics->getBufferPointer() == nullptr ||
            diagnostics->getBufferSize() == 0) {
        return;
    }

    const std::string_view message{
        static_cast<const char*>(diagnostics->getBufferPointer()),
        diagnostics->getBufferSize(),
    };
    if (failed) {
        getLogChannel().error("Slang: {}", message);
    } else {
        getLogChannel().warn("Slang: {}", message);
    }
}

void checkSlangResult(SlangResult result, const char* operation,
        slang::IBlob* diagnostics = nullptr) {
    const bool failed = SLANG_FAILED(result);
    reportDiagnostics(diagnostics, failed);
    if (failed) {
        throw std::runtime_error(std::string{ operation } + " failed.");
    }
}

std::string readSource(const std::filesystem::path& path) {
    std::ifstream stream{ path, std::ios::binary };
    if (!stream) {
        throw std::runtime_error("Failed to open Slang shader: " + path.string());
    }
    return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{} };
}

std::vector<uint32_t> copySpirv(slang::IBlob& code, const char* stage) {
    if (code.getBufferPointer() == nullptr || code.getBufferSize() == 0 ||
            code.getBufferSize() % sizeof(uint32_t) != 0) {
        throw std::runtime_error(
                std::string{ "Slang produced invalid SPIR-V for the " } + stage + " stage.");
    }

    std::vector<uint32_t> spirv(code.getBufferSize() / sizeof(uint32_t));
    std::memcpy(spirv.data(), code.getBufferPointer(), code.getBufferSize());
    return spirv;
}

ShaderResourceType reflectResourceType(slang::BindingType type) {
    switch (type) {
        case slang::BindingType::Sampler:
            return ShaderResourceType::Sampler;
        case slang::BindingType::Texture:
            return ShaderResourceType::SampledImage;
        case slang::BindingType::MutableTexture:
            return ShaderResourceType::StorageImage;
        case slang::BindingType::ConstantBuffer:
            return ShaderResourceType::UniformBuffer;
        case slang::BindingType::RawBuffer:
            return ShaderResourceType::StorageBuffer;
        case slang::BindingType::MutableRawBuffer:
            return ShaderResourceType::StorageBuffer;
        case slang::BindingType::TypedBuffer:
            return ShaderResourceType::UniformTexelBuffer;
        case slang::BindingType::MutableTypedBuffer:
            return ShaderResourceType::StorageTexelBuffer;
        case slang::BindingType::CombinedTextureSampler:
            return ShaderResourceType::CombinedImageSampler;
        default:
            throw std::runtime_error("Slang reflected an unsupported resource binding type.");
    }
}

ShaderReflection reflectProgram(slang::ProgramLayout& layout, ShaderStageMask stages) {
    ShaderReflection reflection;
    for (uint32_t parameter_index = 0; parameter_index < layout.getParameterCount();
            ++parameter_index) {
        slang::VariableLayoutReflection* parameter = layout.getParameterByIndex(parameter_index);
        slang::TypeLayoutReflection* type_layout = parameter->getTypeLayout();
        const SlangInt range_count = type_layout->getBindingRangeCount();
        for (SlangInt range_index = 0; range_index < range_count; ++range_index) {
            const slang::BindingType binding_type = type_layout->getBindingRangeType(range_index);
            if (binding_type == slang::BindingType::PushConstant) {
                slang::TypeLayoutReflection* value_layout = type_layout->getElementTypeLayout();
                const size_t size =
                        value_layout == nullptr
                                ? type_layout->getSize(slang::ParameterCategory::Uniform)
                                : value_layout->getSize(slang::ParameterCategory::Uniform);
                if (size == 0 || size == SLANG_UNBOUNDED_SIZE || size == SLANG_UNKNOWN_SIZE ||
                        size > std::numeric_limits<uint32_t>::max()) {
                    throw std::runtime_error("Slang reflected an invalid push-constant size.");
                }
                reflection.push_constants.push_back({ 0, static_cast<uint32_t>(size), stages });
                continue;
            }

            const SlangInt reflected_count = type_layout->getBindingRangeBindingCount(range_index);
            if (reflected_count <= 0 ||
                    static_cast<size_t>(reflected_count) == SLANG_UNBOUNDED_SIZE ||
                    static_cast<size_t>(reflected_count) == SLANG_UNKNOWN_SIZE ||
                    static_cast<uint64_t>(reflected_count) > std::numeric_limits<uint32_t>::max()) {
                throw std::runtime_error(
                        "Unbounded or unresolved descriptor arrays are not enabled yet.");
            }
            reflection.bindings.push_back({
                parameter->getName(),
                reflectResourceType(binding_type),
                parameter->getBindingSpace(),
                parameter->getBindingIndex(),
                static_cast<uint32_t>(reflected_count),
                stages,
            });
        }
    }
    return reflection;
}

void logReflection(const ShaderReflection& reflection) {
    for (const auto& binding: reflection.bindings) {
        getLogChannel().debug("Reflected shader resource '{}' at set {}, binding {} (count {})",
                binding.name, binding.set, binding.binding, binding.count);
    }
    for (const auto& range: reflection.push_constants) {
        getLogChannel().debug("Reflected push constants (offset {}, size {})", range.offset,
                range.size);
    }
}

struct GlobalCompileState {
    GlobalCompileState() {
        checkSlangResult(slang::createGlobalSession(global_session.writeRef()),
                "slang::createGlobalSession");
    }

    std::mutex mutex;
    Slang::ComPtr<slang::IGlobalSession> global_session;
};

GlobalCompileState& globalCompileState() {
    static GlobalCompileState state;
    return state;
}

struct CompileContext {
    std::unique_lock<std::mutex> lock;
    Slang::ComPtr<slang::IGlobalSession> global_session;
    Slang::ComPtr<slang::ISession> session;
    Slang::ComPtr<slang::IModule> module;
};

CompileContext createCompileContext(const std::filesystem::path& source_path) {
    GlobalCompileState& global_state = globalCompileState();
    CompileContext context;
    context.lock = std::unique_lock{ global_state.mutex };
    context.global_session = global_state.global_session;

    slang::TargetDesc target_desc{};
    target_desc.format = SLANG_SPIRV;
    target_desc.profile = context.global_session->findProfile("spirv_1_6");
    target_desc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
    if (target_desc.profile == SLANG_PROFILE_UNKNOWN) {
        throw std::runtime_error("Slang does not provide the spirv_1_6 profile.");
    }

    const std::string search_path = source_path.parent_path().string();
    const char* search_paths[] = { search_path.c_str() };
    slang::SessionDesc session_desc{};
    session_desc.targets = &target_desc;
    session_desc.targetCount = 1;
    session_desc.searchPaths = search_paths;
    session_desc.searchPathCount = 1;
    session_desc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

    checkSlangResult(
            context.global_session->createSession(session_desc, context.session.writeRef()),
            "IGlobalSession::createSession");

    const std::string source = readSource(source_path);
    const std::string module_name = source_path.stem().string();
    const std::string source_name = source_path.string();
    Slang::ComPtr<slang::IBlob> diagnostics;
    context.module = context.session->loadModuleFromSourceString(module_name.c_str(),
            source_name.c_str(), source.c_str(), diagnostics.writeRef());
    reportDiagnostics(diagnostics, context.module.get() == nullptr);
    if (!context.module) {
        throw std::runtime_error("Slang failed to load shader module: " + source_path.string());
    }
    return context;
}

} // namespace

CompiledGraphicsProgram SlangCompiler::compileGraphics(const GraphicsShaderCompileInfo& info) {
    if (info.source_path.empty()) {
        throw std::invalid_argument("A Slang shader source path is required.");
    }

    CompileContext context = createCompileContext(info.source_path);

    Slang::ComPtr<slang::IBlob> diagnostics;
    Slang::ComPtr<slang::IEntryPoint> vertex_entry;
    diagnostics.setNull();
    checkSlangResult(context.module->findEntryPointByName(info.vertex_entry_point.c_str(),
                             vertex_entry.writeRef()),
            "IModule::findEntryPointByName(vertex)");

    Slang::ComPtr<slang::IEntryPoint> fragment_entry;
    checkSlangResult(context.module->findEntryPointByName(info.fragment_entry_point.c_str(),
                             fragment_entry.writeRef()),
            "IModule::findEntryPointByName(fragment)");

    slang::IComponentType* components[] = { context.module.get(), vertex_entry.get(),
        fragment_entry.get() };
    Slang::ComPtr<slang::IComponentType> composite;
    diagnostics.setNull();
    checkSlangResult(context.session->createCompositeComponentType(components, 3,
                             composite.writeRef(), diagnostics.writeRef()),
            "ISession::createCompositeComponentType", diagnostics);

    Slang::ComPtr<slang::IComponentType> linked_program;
    diagnostics.setNull();
    checkSlangResult(composite->link(linked_program.writeRef(), diagnostics.writeRef()),
            "IComponentType::link", diagnostics);

    Slang::ComPtr<slang::IBlob> vertex_code;
    diagnostics.setNull();
    checkSlangResult(
            linked_program->getEntryPointCode(0, 0, vertex_code.writeRef(), diagnostics.writeRef()),
            "IComponentType::getEntryPointCode(vertex)", diagnostics);

    Slang::ComPtr<slang::IBlob> fragment_code;
    diagnostics.setNull();
    checkSlangResult(linked_program->getEntryPointCode(1, 0, fragment_code.writeRef(),
                             diagnostics.writeRef()),
            "IComponentType::getEntryPointCode(fragment)", diagnostics);

    CompiledGraphicsProgram program;
    program.vertex.entry_point = "main";
    program.vertex.spirv = copySpirv(*vertex_code, "vertex");
    program.fragment.entry_point = "main";
    program.fragment.spirv = copySpirv(*fragment_code, "fragment");
    diagnostics.setNull();
    slang::ProgramLayout* layout = linked_program->getLayout(0, diagnostics.writeRef());
    reportDiagnostics(diagnostics, layout == nullptr);
    if (layout == nullptr) {
        throw std::runtime_error("Slang failed to reflect the graphics program.");
    }
    program.reflection =
            reflectProgram(*layout, ShaderStageMask::Vertex | ShaderStageMask::Fragment);
    logReflection(program.reflection);
    getLogChannel().info("Compiled Slang shader '{}' to SPIR-V", info.source_path.string());
    return program;
}

CompiledComputeProgram SlangCompiler::compileCompute(const ComputeShaderCompileInfo& info) {
    if (info.source_path.empty()) {
        throw std::invalid_argument("A Slang compute shader source path is required.");
    }

    CompileContext context = createCompileContext(info.source_path);

    Slang::ComPtr<slang::IBlob> diagnostics;
    Slang::ComPtr<slang::IEntryPoint> compute_entry;
    diagnostics.setNull();
    checkSlangResult(context.module->findEntryPointByName(info.compute_entry_point.c_str(),
                             compute_entry.writeRef()),
            "IModule::findEntryPointByName(compute)");

    slang::IComponentType* components[] = { context.module.get(), compute_entry.get() };
    Slang::ComPtr<slang::IComponentType> composite;
    diagnostics.setNull();
    checkSlangResult(context.session->createCompositeComponentType(components, 2,
                             composite.writeRef(), diagnostics.writeRef()),
            "ISession::createCompositeComponentType(compute)", diagnostics);

    Slang::ComPtr<slang::IComponentType> linked_program;
    diagnostics.setNull();
    checkSlangResult(composite->link(linked_program.writeRef(), diagnostics.writeRef()),
            "IComponentType::link(compute)", diagnostics);

    Slang::ComPtr<slang::IBlob> compute_code;
    diagnostics.setNull();
    checkSlangResult(linked_program->getEntryPointCode(0, 0, compute_code.writeRef(),
                             diagnostics.writeRef()),
            "IComponentType::getEntryPointCode(compute)", diagnostics);

    diagnostics.setNull();
    slang::ProgramLayout* layout = linked_program->getLayout(0, diagnostics.writeRef());
    reportDiagnostics(diagnostics, layout == nullptr);
    if (layout == nullptr || layout->getEntryPointCount() != 1) {
        throw std::runtime_error("Slang failed to reflect the compute program.");
    }

    SlangUInt group_size[3] = { 1, 1, 1 };
    layout->getEntryPointByIndex(0)->getComputeThreadGroupSize(3, group_size);
    CompiledComputeProgram program;
    program.compute.entry_point = "main";
    program.compute.spirv = copySpirv(*compute_code, "compute");
    program.reflection = reflectProgram(*layout, ShaderStageMask::Compute);
    program.thread_group_size_x = static_cast<uint32_t>(group_size[0]);
    program.thread_group_size_y = static_cast<uint32_t>(group_size[1]);
    program.thread_group_size_z = static_cast<uint32_t>(group_size[2]);
    logReflection(program.reflection);
    getLogChannel().info("Compiled Slang compute shader '{}' to SPIR-V", info.source_path.string());
    return program;
}

} // namespace arti::renderer
