#include "image_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <limits>
#include <stdexcept>
#include <string>

namespace arti::renderer_showcase {

ImageData loadImageRGBA(const std::filesystem::path& path)
{
    int width = 0;
    int height = 0;
    int source_channels = 0;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &source_channels, STBI_rgb_alpha);
    if (pixels == nullptr) {
        const char* reason = stbi_failure_reason();
        throw std::runtime_error(
            "Failed to load image '" + path.string() + "': " + (reason == nullptr ? "unknown error" : reason));
    }

    if (width <= 0 || height <= 0 ||
        static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4 > std::numeric_limits<size_t>::max()) {
        stbi_image_free(pixels);
        throw std::runtime_error("Image dimensions are invalid: " + path.string());
    }

    ImageData image;
    image.width = static_cast<uint32_t>(width);
    image.height = static_cast<uint32_t>(height);
    const size_t byte_count = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    const auto* begin = reinterpret_cast<const std::byte*>(pixels);
    image.rgba_pixels.assign(begin, begin + byte_count);
    stbi_image_free(pixels);
    return image;
}

} // namespace arti::renderer_showcase
