#pragma once
#include "application_event.h"
#include "event.h"
#include "layer_stack.h"
#include "log.h"
#include "window.h"

#include <cstdint>

#include <memory>
#include <string>

namespace arti::core {

struct ApplicationCreateInfo {
    std::string name{"ArtiChoco"};
    std::string log_channel{"ArtiApp"};
    uint32_t width{1'280};
    uint32_t height{720};
    WindowFactory window_factory{createHeadlessWindow};
};

class Application {
public:
    explicit Application(const ApplicationCreateInfo& info);
    ~Application();

    static Application& get();

    const Logger::Channel& getLogChannel() const;
    Window& getWindow();
    const Window& getWindow() const;

    void run();
    void close();
    void pushLayer(std::unique_ptr<Layer> layer);
    void pushOverlay(std::unique_ptr<Layer> overlay);

private:
    void initialize(const ApplicationCreateInfo& info);
    void shutdown();
    void onEvent(Event& event);
    bool onWindowClose(WindowCloseEvent& event);
    bool onWindowResize(WindowResizeEvent& event);

private:
    bool m_is_running{true};

    Logger::ChannelHandle m_log_channel;
    std::unique_ptr<Window> m_window;
    LayerStack m_layer_stack;
};

Application* createApplication(int argc, char** argv);
} // namespace arti::core
