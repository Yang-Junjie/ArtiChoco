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
    bool smoke_nvrhi = false;
    bool smoke_render = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        use_headless_window |= argument == "--headless";
        smoke_sdl |= argument == "--smoke-sdl";
        smoke_nvrhi |= argument == "--smoke-nvrhi";
        smoke_render |= argument == "--smoke-render";
    }

    use_headless_window &= !(smoke_nvrhi || smoke_render);
    const bool enable_renderer = smoke_render || (!use_headless_window && !smoke_sdl && !smoke_nvrhi);

    if (!use_headless_window) {
        info.window_factory = platform::createSDLWindow;
    }

    auto* app = new Application(info);
    app->pushLayer(std::make_unique<test_app::TestAppLayer>(
            enable_renderer, smoke_render, smoke_nvrhi));
    if (smoke_sdl) {
        app->close();
    }

    return app;
}

} // namespace arti::core
