#include "application.h"
#include "artichoco/platform/window/window_factory.h"
#include "instancing_layer.h"

#include <memory>
#include <string_view>

namespace arti::core {

Application* createApplication(int argc, char** argv) {
    ApplicationCreateInfo info;
    info.name = "ArtiChoco Instancing";
    info.log_channel = "Instancing";
    info.width = 1'280;
    info.height = 720;
    info.window_factory = platform::createSDLWindow;

    bool smoke = false;
    for (int index = 1; index < argc; ++index) {
        smoke |= std::string_view{ argv[index] } == "--smoke";
    }

    auto* app = new Application(info);
    app->pushLayer(std::make_unique<instancing::InstancingLayer>(smoke));
    return app;
}

} // namespace arti::core
