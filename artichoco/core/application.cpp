#include "application.h"
#include "layer.h"
#include "log.h"

#include <chrono>

namespace arti::core {
namespace {
Application* s_instance = nullptr;
} // namespace

Application::Application(const ApplicationCreateInfo& info)
{
    ARTI_ASSERT(s_instance == nullptr, "Application already exists.");

    s_instance = this;
    initialize(info);
}

Application::~Application()
{
    shutdown();
}

Application& Application::get()
{
    ARTI_ASSERT(s_instance != nullptr, "Application instance is null.");

    return *s_instance;
}

const Logger::Channel& Application::getLogChannel() const
{
    ARTI_ASSERT(m_log_channel, "Application log channel is null.");
    return *m_log_channel;
}

void Application::run()
{
    ARTI_ASSERT(m_window, "Application window is null.");

    ARTI_CORE_INFO("Application main loop started");
    auto lastFrameTime = std::chrono::steady_clock::now();

    while (m_is_running && !m_window->shouldClose()) {
        const auto currentFrameTime = std::chrono::steady_clock::now();
        const std::chrono::duration<float> deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        m_window->onUpdate();

        for (auto& layer : m_layer_stack) {
            layer->onUpdate(deltaTime.count());
        }

        for (auto& layer : m_layer_stack) {
            layer->onRender();
        }
    }

    ARTI_CORE_INFO("Application main loop finished");
}

void Application::close()
{
    ARTI_CORE_DEBUG("Application close requested");
    m_is_running = false;
}

void Application::pushLayer(std::unique_ptr<Layer> layer)
{
    ARTI_ASSERT(layer, "Cannot push a null layer.");
    m_layer_stack.pushLayer(std::move(layer));
}

void Application::pushOverlay(std::unique_ptr<Layer> overlay)
{
    ARTI_ASSERT(overlay, "Cannot push a null overlay.");
    m_layer_stack.pushOverlay(std::move(overlay));
}

void Application::initialize(const ApplicationCreateInfo& info)
{
    ARTI_ASSERT(!info.log_channel.empty(), "Application log channel name cannot be empty.");
    ARTI_ASSERT(info.width > 0 && info.height > 0, "Application size must be non-zero.");
    ARTI_ASSERT(info.window_factory, "Application window factory is null.");

    m_log_channel = Logger::registerChannel(info.log_channel);
    ARTI_CORE_INFO("Initializing application '{}' ({}x{})", info.name, info.width, info.height);

    WindowCreateInfo window_info;
    window_info.width = info.width;
    window_info.height = info.height;
    window_info.title = info.name;

    m_window = info.window_factory(window_info);
    ARTI_ASSERT(m_window, "Failed to create window.");
    m_window->setEventCallback([this](Event& event) {
        onEvent(event);
    });
    m_window->init();

    ARTI_CORE_INFO("Application initialized");
}

void Application::shutdown()
{
    ARTI_CORE_INFO("Application shutdown started");

    m_window.reset();

    ARTI_CORE_INFO("Application shutdown finished");
}

void Application::onEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent& e) {
        return onWindowClose(e);
    });
    dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
        return onWindowResize(e);
    });

    for (auto it = m_layer_stack.rbegin(); it != m_layer_stack.rend(); ++it) {
        if (event.m_handled) {
            break;
        }

        (*it)->onEvent(event);
    }
}

bool Application::onWindowClose(WindowCloseEvent&)
{
    ARTI_CORE_INFO("Window close event received");
    close();
    return true;
}

bool Application::onWindowResize(WindowResizeEvent& event)
{
    ARTI_CORE_DEBUG("Window resized to {}x{}", event.getWidth(), event.getHeight());
    return false;
}
} // namespace arti::core
