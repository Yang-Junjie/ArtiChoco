#include "application.h"
#include "artichoco/platform/window/window_factory.h"
#include "hello_triangle_layer.h"

#include <memory>
#include <string_view>

namespace arti::core {

Application* createApplication(int argc, char** argv)
{
    ApplicationCreateInfo info;
    info.name = "ArtiChoco Hello Triangle";
    info.log_channel = "HelloTriangle";
    info.width = 1'280;
    info.height = 720;
    info.window_factory = platform::createSDLWindow;

    bool smoke = false;
    for (int index = 1; index < argc; ++index) {
        smoke |= std::string_view{argv[index]} == "--smoke";
    }

    auto* app = new Application(info);
    app->pushLayer(std::make_unique<hello_triangle::HelloTriangleLayer>(smoke));
    return app;
}

} // namespace arti::core
