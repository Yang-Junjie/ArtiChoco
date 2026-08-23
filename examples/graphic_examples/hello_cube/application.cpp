#include "application.h"
#include "artichoco/platform/window/window_factory.h"
#include "hello_cube_layer.h"

#include <memory>
#include <string_view>

namespace arti::core {

Application* createApplication(int argc, char** argv) {
    ApplicationCreateInfo info;
    info.name = "ArtiChoco Hello Cube";
    info.log_channel = "HelloCube";
    info.width = 1'280;
    info.height = 720;
    info.window_factory = platform::createSDLWindow;

    bool smoke = false;
    for (int index = 1; index < argc; ++index) {
        smoke |= std::string_view{ argv[index] } == "--smoke";
    }

    auto* app = new Application(info);
    app->pushLayer(std::make_unique<hello_cube::HelloCubeLayer>(smoke));
    return app;
}

} // namespace arti::core
