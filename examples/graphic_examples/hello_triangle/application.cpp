#include "application.h"
#include "artichoco/platform/window/window_factory.h"
#include "hello_triangle_layer.h"

#include <memory>

namespace arti::core {

Application* createApplication(int, char**) {
    ApplicationCreateInfo info;
    info.name = "Hello Triangle";
    info.log_channel = "HelloTriangle";
    info.width = 960;
    info.height = 540;
    info.window_factory = platform::createSDLWindow;

    auto* application = new Application(info);
    application->pushLayer(std::make_unique<hello_triangle::HelloTriangleLayer>());
    return application;
}

} // namespace arti::core
