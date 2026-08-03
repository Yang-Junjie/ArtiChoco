#pragma once
#include "event.h"

#include <cstdint>

#include <functional>
#include <memory>
#include <string>

namespace arti::core {

class WindowCreateInfo {
public:
    uint32_t width{800};
    uint32_t height{600};
    std::string title{"ArtiChoco"};
};

class Window;
using WindowFactory = std::function<std::unique_ptr<Window>(const WindowCreateInfo&)>;

std::unique_ptr<Window> createHeadlessWindow(const WindowCreateInfo& info);

class Window {
public:
    using EventCallbackFn = std::function<void(Event&)>;
    virtual ~Window() = default;

    virtual void init() = 0;
    virtual void onUpdate() = 0;
    virtual void setEventCallback(const EventCallbackFn& callback) = 0;
    virtual bool shouldClose() = 0;
    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual uint32_t getFramebufferWidth() const = 0;
    virtual uint32_t getFramebufferHeight() const = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
};

} // namespace arti::core
