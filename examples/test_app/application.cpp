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
    bool close_immediately = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        use_headless_window |= argument == "--headless";
        close_immediately |= argument == "--smoke-sdl";
    }

    if (!use_headless_window) {
        info.window_factory = platform::createSDLWindow;
    }

    auto* app = new Application(info);
    app->pushLayer(std::make_unique<test_app::TestAppLayer>());
    if (close_immediately) {
        app->close();
    }

    return app;
}

} // namespace arti::core
