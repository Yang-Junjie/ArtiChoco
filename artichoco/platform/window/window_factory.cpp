#include "sdl_window.h"
#include "window_factory.h"

#include <memory>

namespace arti::platform {

std::unique_ptr<core::Window> createSDLWindow(const core::WindowCreateInfo& info)
{
    return std::make_unique<SDLWindow>(info);
}

} // namespace arti::platform
