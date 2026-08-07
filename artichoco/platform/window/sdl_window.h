#pragma once
#include "artichoco/core/io/input.h"
#include "artichoco/core/window.h"

#include <SDL3/SDL.h>

#include <atomic>

namespace arti::platform {

class SDLWindow final : public core::Window, public core::InputProvider {
public:
    explicit SDLWindow(const core::WindowCreateInfo& info);
    ~SDLWindow() override;

    SDLWindow(const SDLWindow&) = delete;
    SDLWindow& operator=(const SDLWindow&) = delete;

    void init() override;
    void onUpdate() override;
    void setEventCallback(const EventCallbackFn& callback) override;
    bool shouldClose() override;
    uint32_t getWidth() const override;
    uint32_t getHeight() const override;
    uint32_t getFramebufferWidth() const override;
    uint32_t getFramebufferHeight() const override;
    void resize(uint32_t width, uint32_t height) override;

    bool isKeyPressed(core::KeyCode key) const override;
    bool isMouseButtonPressed(core::MouseCode button) const override;
    glm::vec2 getMousePosition() const override;
    core::CursorMode getCursorMode() const override;
    void setCursorMode(core::CursorMode mode) override;
    void setMousePosition(float x, float y) override;
    void setRawMouseMotion(bool enabled) override;

    SDL_Window* nativeHandle() const noexcept;

private:
    void handleEvent(const SDL_Event& event);
    void updateFramebufferSize();

private:
    core::WindowCreateInfo m_info;
    EventCallbackFn m_event_callback;
    SDL_Window* m_window{nullptr};
    SDL_WindowID m_window_id{0};
    std::atomic<uint32_t> m_framebuffer_width{0};
    std::atomic<uint32_t> m_framebuffer_height{0};
    core::CursorMode m_cursor_mode{core::CursorMode::Normal};
    bool m_should_close{false};
    bool m_platform_acquired{false};
    bool m_text_input_active{false};
};

} // namespace arti::platform
