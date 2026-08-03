#include "slang_compiler.h"

#include "renderer_log.h"

#include <slang/slang-com-ptr.h>
#include <slang/slang.h>

#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace arti::renderer {
namespace {

void reportDiagnostics(slang::IBlob* diagnostics, bool failed)
{
    if (diagnostics == nullptr || diagnostics->getBufferPointer() == nullptr || diagnostics->getBufferSize() == 0) {
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

void checkSlangResult(SlangResult result, const char* operation, slang::IBlob* diagnostics = nullptr)
{
    const bool failed = SLANG_FAILED(result);
    reportDiagnostics(diagnostics, failed);
    if (failed) {
        throw std::runtime_error(std::string{operation} + " failed.");
    }
}

std::string readSource(const std::filesystem::path& path)
{
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        throw std::runtime_error("Failed to open Slang shader: " + path.string());
    }
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

std::vector<uint32_t> copySpirv(slang::IBlob& code, const char* stage)
{
    if (code.getBufferPointer() == nullptr || code.getBufferSize() == 0 || code.getBufferSize() % sizeof(uint32_t) != 0) {
        throw std::runtime_error(std::string{"Slang produced invalid SPIR-V for the "} + stage + " stage.");
    }

    std::vector<uint32_t> spirv(code.getBufferSize() / sizeof(uint32_t));
    std::memcpy(spirv.data(), code.getBufferPointer(), code.getBufferSize());
    return spirv;
}

} // namespace

CompiledGraphicsProgram SlangCompiler::compileGraphics(
    const std::filesystem::path& source_path,
    std::string vertex_entry_point,
    std::string fragment_entry_point)
{
    if (source_path.empty()) {
        throw std::invalid_argument("A Slang shader source path is required.");
    }

    Slang::ComPtr<slang::IGlobalSession> global_session;
    checkSlangResult(slang::createGlobalSession(global_session.writeRef()), "slang::createGlobalSession");

    slang::TargetDesc target_desc{};
    target_desc.format = SLANG_SPIRV;
    target_desc.profile = global_session->findProfile("spirv_1_6");
    target_desc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
    if (target_desc.profile == SLANG_PROFILE_UNKNOWN) {
        throw std::runtime_error("Slang does not provide the spirv_1_6 profile.");
    }

    const std::string search_path = source_path.parent_path().string();
    const char* search_paths[] = {search_path.c_str()};
    slang::SessionDesc session_desc{};
    session_desc.targets = &target_desc;
    session_desc.targetCount = 1;
    session_desc.searchPaths = search_paths;
    session_desc.searchPathCount = 1;
    session_desc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

    Slang::ComPtr<slang::ISession> session;
    checkSlangResult(global_session->createSession(session_desc, session.writeRef()), "IGlobalSession::createSession");

    const std::string source = readSource(source_path);
    const std::string module_name = source_path.stem().string();
    const std::string source_name = source_path.string();
    Slang::ComPtr<slang::IBlob> diagnostics;
    Slang::ComPtr<slang::IModule> module;
    module = session->loadModuleFromSourceString(
        module_name.c_str(), source_name.c_str(), source.c_str(), diagnostics.writeRef());
    reportDiagnostics(diagnostics, module.get() == nullptr);
    if (!module) {
        throw std::runtime_error("Slang failed to load shader module: " + source_path.string());
    }

    Slang::ComPtr<slang::IEntryPoint> vertex_entry;
    diagnostics.setNull();
    checkSlangResult(
        module->findEntryPointByName(vertex_entry_point.c_str(), vertex_entry.writeRef()),
        "IModule::findEntryPointByName(vertex)");

    Slang::ComPtr<slang::IEntryPoint> fragment_entry;
    checkSlangResult(
        module->findEntryPointByName(fragment_entry_point.c_str(), fragment_entry.writeRef()),
        "IModule::findEntryPointByName(fragment)");

    slang::IComponentType* components[] = {module.get(), vertex_entry.get(), fragment_entry.get()};
    Slang::ComPtr<slang::IComponentType> composite;
    diagnostics.setNull();
    checkSlangResult(
        session->createCompositeComponentType(components, 3, composite.writeRef(), diagnostics.writeRef()),
        "ISession::createCompositeComponentType",
        diagnostics);

    Slang::ComPtr<slang::IComponentType> linked_program;
    diagnostics.setNull();
    checkSlangResult(
        composite->link(linked_program.writeRef(), diagnostics.writeRef()),
        "IComponentType::link",
        diagnostics);

    Slang::ComPtr<slang::IBlob> vertex_code;
    diagnostics.setNull();
    checkSlangResult(
        linked_program->getEntryPointCode(0, 0, vertex_code.writeRef(), diagnostics.writeRef()),
        "IComponentType::getEntryPointCode(vertex)",
        diagnostics);

    Slang::ComPtr<slang::IBlob> fragment_code;
    diagnostics.setNull();
    checkSlangResult(
        linked_program->getEntryPointCode(1, 0, fragment_code.writeRef(), diagnostics.writeRef()),
        "IComponentType::getEntryPointCode(fragment)",
        diagnostics);

    CompiledGraphicsProgram program;
    program.vertex.entry_point = "main";
    program.vertex.spirv = copySpirv(*vertex_code, "vertex");
    program.fragment.entry_point = "main";
    program.fragment.spirv = copySpirv(*fragment_code, "fragment");
    getLogChannel().info("Compiled Slang shader '{}' to SPIR-V", source_path.string());
    return program;
}

} // namespace arti::renderer
