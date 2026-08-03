#include "artichoco/platform/platform_log.h"
#include "sdl_platform.h"

#include <cstddef>

#include <SDL3/SDL.h>
#include <stdexcept>
#include <string>

namespace arti::platform {
namespace {

std::size_t s_reference_count = 0;

} // namespace

void SDLPlatform::acquire()
{
    if (s_reference_count > 0) {
        ++s_reference_count;
        return;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        const std::string message = std::string{"Failed to initialize SDL: "} + SDL_GetError();
        getLogChannel().fatal("{}", message);
        throw std::runtime_error(message);
    }

    s_reference_count = 1;
    getLogChannel().info("SDL initialized");
}

void SDLPlatform::release()
{
    if (s_reference_count == 0) {
        return;
    }

    --s_reference_count;
    if (s_reference_count == 0) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        getLogChannel().info("SDL shutdown");
    }
}

} // namespace arti::platform
