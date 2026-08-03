#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace arti::test_app {

struct ImageData {
    uint32_t width{0};
    uint32_t height{0};
    std::vector<std::byte> rgba_pixels;
};

ImageData loadImageRGBA(const std::filesystem::path& path);

} // namespace arti::test_app
