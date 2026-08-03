#include "application.h"
#include "artichoco/platform/window/window_factory.h"
#include "test_app_layer.h"

#include <memory>
#include <string_view>

namespace arti::core {

Application* createApplication(int argc, char** argv)
{
    ApplicationCreateInfo info;
    info.name = "Test App";
    info.log_channel = "TestApp";
    info.width = 1'280;
    info.height = 720;

    bool use_headless_window = false;
    bool smoke_sdl = false;
    bool smoke_vulkan = false;
    bool smoke_render = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        use_headless_window |= argument == "--headless";
        smoke_sdl |= argument == "--smoke-sdl";
        smoke_vulkan |= argument == "--smoke-vulkan";
        smoke_render |= argument == "--smoke-render";
    }

    smoke_vulkan &= !smoke_render;
    use_headless_window &= !(smoke_vulkan || smoke_render);
    const bool enable_renderer = smoke_render || (!use_headless_window && !smoke_sdl && !smoke_vulkan);

    if (!use_headless_window) {
        info.window_factory = platform::createSDLWindow;
    }

    auto* app = new Application(info);
    app->pushLayer(std::make_unique<test_app::TestAppLayer>(smoke_vulkan, enable_renderer, smoke_render));
    if (smoke_sdl || smoke_vulkan) {
        app->close();
    }

    return app;
}

} // namespace arti::core
