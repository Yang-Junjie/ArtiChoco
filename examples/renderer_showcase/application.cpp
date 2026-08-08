#include "application.h"
#include "artichoco/platform/window/window_factory.h"
#include "renderer_showcase_layer.h"

#include <memory>
#include <string_view>

namespace arti::core {

Application* createApplication(int argc, char** argv)
{
    ApplicationCreateInfo info;
    info.name = "Renderer Showcase";
    info.log_channel = "RendererShowcase";
    info.width = 960;
    info.height = 540;
    info.window_factory = platform::createSDLWindow;

    bool smoke_render = false;
    for (int index = 1; index < argc; ++index) {
        smoke_render |= std::string_view{argv[index]} == "--smoke-render";
    }

    auto* application = new Application(info);
    application->pushLayer(std::make_unique<renderer_showcase::RendererShowcaseLayer>(smoke_render));
    return application;
}

} // namespace arti::core
