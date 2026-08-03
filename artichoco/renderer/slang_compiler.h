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

struct CompiledGraphicsProgram {
    CompiledShaderStage vertex;
    CompiledShaderStage fragment;
};

class SlangCompiler {
public:
    static CompiledGraphicsProgram compileGraphics(
        const std::filesystem::path& source_path,
        std::string vertex_entry_point,
        std::string fragment_entry_point);
};

} // namespace arti::renderer
