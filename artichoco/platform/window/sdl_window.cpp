#include "artichoco/core/event/application_event.h"
#include "artichoco/core/event/key_event.h"
#include "artichoco/core/event/mouse_event.h"
#include "artichoco/platform/platform_log.h"
#include "sdl_platform.h"
#include "sdl_window.h"

#include <cstddef>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace arti::platform {
namespace {

struct KeyMapping {
    core::KeyCode key;
    SDL_Scancode scancode;
};

constexpr KeyMapping key_mappings[] = {
    {core::KeyCode::LeftShift, SDL_SCANCODE_LSHIFT},
    {core::KeyCode::RightShift, SDL_SCANCODE_RSHIFT},
    {core::KeyCode::LeftControl, SDL_SCANCODE_LCTRL},
    {core::KeyCode::RightControl, SDL_SCANCODE_RCTRL},
    {core::KeyCode::LeftAlt, SDL_SCANCODE_LALT},
    {core::KeyCode::RightAlt, SDL_SCANCODE_RALT},
    {core::KeyCode::Space, SDL_SCANCODE_SPACE},
    {core::KeyCode::Enter, SDL_SCANCODE_RETURN},
    {core::KeyCode::Delete, SDL_SCANCODE_DELETE},
    {core::KeyCode::Escape, SDL_SCANCODE_ESCAPE},
    {core::KeyCode::F1, SDL_SCANCODE_F1},
    {core::KeyCode::F2, SDL_SCANCODE_F2},
    {core::KeyCode::F3, SDL_SCANCODE_F3},
    {core::KeyCode::F4, SDL_SCANCODE_F4},
    {core::KeyCode::F5, SDL_SCANCODE_F5},
    {core::KeyCode::F6, SDL_SCANCODE_F6},
    {core::KeyCode::F7, SDL_SCANCODE_F7},
    {core::KeyCode::F8, SDL_SCANCODE_F8},
    {core::KeyCode::F9, SDL_SCANCODE_F9},
    {core::KeyCode::F10, SDL_SCANCODE_F10},
    {core::KeyCode::F11, SDL_SCANCODE_F11},
    {core::KeyCode::F12, SDL_SCANCODE_F12},
    {core::KeyCode::Up, SDL_SCANCODE_UP},
    {core::KeyCode::Down, SDL_SCANCODE_DOWN},
    {core::KeyCode::Left, SDL_SCANCODE_LEFT},
    {core::KeyCode::Right, SDL_SCANCODE_RIGHT},
    {core::KeyCode::Backspace, SDL_SCANCODE_BACKSPACE},
    {core::KeyCode::A, SDL_SCANCODE_A},
    {core::KeyCode::B, SDL_SCANCODE_B},
    {core::KeyCode::C, SDL_SCANCODE_C},
    {core::KeyCode::D, SDL_SCANCODE_D},
    {core::KeyCode::E, SDL_SCANCODE_E},
    {core::KeyCode::F, SDL_SCANCODE_F},
    {core::KeyCode::G, SDL_SCANCODE_G},
    {core::KeyCode::H, SDL_SCANCODE_H},
    {core::KeyCode::I, SDL_SCANCODE_I},
    {core::KeyCode::J, SDL_SCANCODE_J},
    {core::KeyCode::K, SDL_SCANCODE_K},
    {core::KeyCode::L, SDL_SCANCODE_L},
    {core::KeyCode::M, SDL_SCANCODE_M},
    {core::KeyCode::N, SDL_SCANCODE_N},
    {core::KeyCode::O, SDL_SCANCODE_O},
    {core::KeyCode::P, SDL_SCANCODE_P},
    {core::KeyCode::Q, SDL_SCANCODE_Q},
    {core::KeyCode::R, SDL_SCANCODE_R},
    {core::KeyCode::S, SDL_SCANCODE_S},
    {core::KeyCode::T, SDL_SCANCODE_T},
    {core::KeyCode::U, SDL_SCANCODE_U},
    {core::KeyCode::V, SDL_SCANCODE_V},
    {core::KeyCode::W, SDL_SCANCODE_W},
    {core::KeyCode::X, SDL_SCANCODE_X},
    {core::KeyCode::Y, SDL_SCANCODE_Y},
    {core::KeyCode::Z, SDL_SCANCODE_Z},
};

SDL_Scancode toSDLScancode(core::KeyCode key)
{
    for (const auto& mapping : key_mappings) {
        if (mapping.key == key) {
            return mapping.scancode;
        }
    }

    return SDL_SCANCODE_UNKNOWN;
}

core::KeyCode fromSDLScancode(SDL_Scancode scancode)
{
    for (const auto& mapping : key_mappings) {
        if (mapping.scancode == scancode) {
            return mapping.key;
        }
    }

    return core::KeyCode::None;
}

core::MouseCode fromSDLMouseButton(Uint8 button)
{
    switch (button) {
        case SDL_BUTTON_LEFT:
            return core::MouseCode::Left;
        case SDL_BUTTON_RIGHT:
            return core::MouseCode::Right;
        case SDL_BUTTON_MIDDLE:
            return core::MouseCode::Middle;
        case SDL_BUTTON_X1:
            return core::MouseCode::XButton1;
        case SDL_BUTTON_X2:
            return core::MouseCode::XButton2;
        default:
            return core::MouseCode::None;
    }
}

SDL_MouseButtonFlags toSDLMouseButtonMask(core::MouseCode button)
{
    switch (button) {
        case core::MouseCode::Left:
            return SDL_BUTTON_LMASK;
        case core::MouseCode::Right:
            return SDL_BUTTON_RMASK;
        case core::MouseCode::Middle:
            return SDL_BUTTON_MMASK;
        case core::MouseCode::XButton1:
            return SDL_BUTTON_X1MASK;
        case core::MouseCode::XButton2:
            return SDL_BUTTON_X2MASK;
        case core::MouseCode::None:
            return 0;
    }

    return 0;
}

} // namespace

SDLWindow::SDLWindow(const core::WindowCreateInfo& info)
    : m_info(info)
{}

SDLWindow::~SDLWindow()
{
    if (m_window != nullptr) {
        core::Input::setProvider(nullptr);

        if (m_text_input_active) {
            SDL_StopTextInput(m_window);
        }

        getLogChannel().info("Destroying SDL window '{}'", m_info.title);
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        m_window_id = 0;
    }

    if (m_platform_acquired) {
        SDLPlatform::release();
        m_platform_acquired = false;
    }
}

void SDLWindow::init()
{
    if (m_window != nullptr) {
        return;
    }

    SDLPlatform::acquire();
    m_platform_acquired = true;

    constexpr SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN;
    m_window = SDL_CreateWindow(
        m_info.title.c_str(), static_cast<int>(m_info.width), static_cast<int>(m_info.height), window_flags);

    if (m_window == nullptr) {
        const std::string message = std::string{"Failed to create SDL window: "} + SDL_GetError();
        getLogChannel().fatal("{}", message);
        SDLPlatform::release();
        m_platform_acquired = false;
        throw std::runtime_error(message);
    }

    m_window_id = SDL_GetWindowID(m_window);
    if (m_window_id == 0) {
        const std::string message = std::string{"Failed to resolve SDL window ID: "} + SDL_GetError();
        getLogChannel().fatal("{}", message);
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        SDLPlatform::release();
        m_platform_acquired = false;
        throw std::runtime_error(message);
    }

    updateFramebufferSize();

    m_text_input_active = SDL_StartTextInput(m_window);
    if (!m_text_input_active) {
        getLogChannel().warn("Failed to start SDL text input: {}", SDL_GetError());
    }

    core::Input::setProvider(this);
    getLogChannel().info("Created SDL window '{}' ({}x{})", m_info.title, m_info.width, m_info.height);
}

void SDLWindow::onUpdate()
{
    core::Input::resetFrameState();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        handleEvent(event);
    }
}

void SDLWindow::setEventCallback(const EventCallbackFn& callback)
{
    m_event_callback = callback;
}

bool SDLWindow::shouldClose()
{
    return m_should_close;
}

uint32_t SDLWindow::getWidth() const
{
    return m_info.width;
}

uint32_t SDLWindow::getHeight() const
{
    return m_info.height;
}

uint32_t SDLWindow::getFramebufferWidth() const
{
    return m_framebuffer_width.load(std::memory_order_relaxed);
}

uint32_t SDLWindow::getFramebufferHeight() const
{
    return m_framebuffer_height.load(std::memory_order_relaxed);
}

void SDLWindow::resize(uint32_t width, uint32_t height)
{
    if (!SDL_SetWindowSize(m_window, static_cast<int>(width), static_cast<int>(height))) {
        getLogChannel().error("Failed to resize SDL window: {}", SDL_GetError());
        return;
    }

    m_info.width = width;
    m_info.height = height;
    updateFramebufferSize();
}

bool SDLWindow::isKeyPressed(core::KeyCode key) const
{
    const SDL_Scancode scancode = toSDLScancode(key);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        return false;
    }

    int key_count = 0;
    const bool* keyboard_state = SDL_GetKeyboardState(&key_count);
    return static_cast<int>(scancode) < key_count && keyboard_state[scancode];
}

bool SDLWindow::isMouseButtonPressed(core::MouseCode button) const
{
    const SDL_MouseButtonFlags mask = toSDLMouseButtonMask(button);
    return mask != 0 && (SDL_GetMouseState(nullptr, nullptr) & mask) != 0;
}

glm::vec2 SDLWindow::getMousePosition() const
{
    float x = 0.0f;
    float y = 0.0f;
    SDL_GetMouseState(&x, &y);
    return {x, y};
}

core::CursorMode SDLWindow::getCursorMode() const
{
    return m_cursor_mode;
}

void SDLWindow::setCursorMode(core::CursorMode mode)
{
    bool succeeded = true;

    switch (mode) {
        case core::CursorMode::Normal: {
            const bool relative_mode_set = SDL_SetWindowRelativeMouseMode(m_window, false);
            const bool mouse_grab_set = SDL_SetWindowMouseGrab(m_window, false);
            const bool cursor_shown = SDL_ShowCursor();
            succeeded = relative_mode_set && mouse_grab_set && cursor_shown;
            break;
        }
        case core::CursorMode::Hidden: {
            const bool relative_mode_set = SDL_SetWindowRelativeMouseMode(m_window, false);
            const bool mouse_grab_set = SDL_SetWindowMouseGrab(m_window, false);
            const bool cursor_hidden = SDL_HideCursor();
            succeeded = relative_mode_set && mouse_grab_set && cursor_hidden;
            break;
        }
        case core::CursorMode::Locked: {
            const bool cursor_hidden = SDL_HideCursor();
            const bool mouse_grab_set = SDL_SetWindowMouseGrab(m_window, true);
            const bool relative_mode_set = SDL_SetWindowRelativeMouseMode(m_window, true);
            succeeded = cursor_hidden && mouse_grab_set && relative_mode_set;
            break;
        }
    }

    if (!succeeded) {
        getLogChannel().error("Failed to set SDL cursor mode: {}", SDL_GetError());
        return;
    }

    m_cursor_mode = mode;
}

void SDLWindow::setMousePosition(float x, float y)
{
    SDL_WarpMouseInWindow(m_window, x, y);
}

void SDLWindow::setRawMouseMotion(bool enabled)
{
    if (!SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, enabled ? "0" : "1")) {
        getLogChannel().warn("Failed to configure SDL raw mouse motion: {}", SDL_GetError());
    }
}

SDLWindow::SDLEventObserverId SDLWindow::addSDLEventObserver(SDLEventObserver observer)
{
    if (!observer) {
        return 0;
    }

    const SDLEventObserverId observer_id = m_next_sdl_event_observer_id++;
    m_sdl_event_observers.emplace(observer_id, std::move(observer));
    return observer_id;
}

void SDLWindow::removeSDLEventObserver(SDLEventObserverId observer_id) noexcept
{
    m_sdl_event_observers.erase(observer_id);
}

SDL_Window* SDLWindow::nativeHandle() const noexcept
{
    return m_window;
}

void SDLWindow::updateFramebufferSize()
{
    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(m_window, &width, &height)) {
        getLogChannel().warn("Failed to query SDL framebuffer size: {}", SDL_GetError());
        m_framebuffer_width.store(m_info.width, std::memory_order_relaxed);
        m_framebuffer_height.store(m_info.height, std::memory_order_relaxed);
        return;
    }

    m_framebuffer_width.store(static_cast<uint32_t>(std::max(width, 0)), std::memory_order_relaxed);
    m_framebuffer_height.store(static_cast<uint32_t>(std::max(height, 0)), std::memory_order_relaxed);
}

void SDLWindow::handleEvent(const SDL_Event& event)
{
    std::vector<SDLEventObserver> observers;
    observers.reserve(m_sdl_event_observers.size());
    for (const auto& [observer_id, observer] : m_sdl_event_observers) {
        (void)observer_id;
        observers.push_back(observer);
    }
    for (const SDLEventObserver& observer : observers) {
        observer(event);
    }

    const auto dispatch = [this](core::Event& translated_event) {
        if (m_event_callback) {
            m_event_callback(translated_event);
        }
    };

    switch (event.type) {
        case SDL_EVENT_QUIT: {
            m_should_close = true;
            core::WindowCloseEvent translated_event;
            dispatch(translated_event);
            break;
        }
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            if (event.window.windowID != m_window_id) {
                break;
            }

            m_should_close = true;
            core::WindowCloseEvent translated_event;
            dispatch(translated_event);
            break;
        }
        case SDL_EVENT_WINDOW_RESIZED: {
            if (event.window.windowID != m_window_id) {
                break;
            }

            m_info.width = static_cast<uint32_t>(std::max(event.window.data1, 0));
            m_info.height = static_cast<uint32_t>(std::max(event.window.data2, 0));
            core::WindowResizeEvent translated_event{m_info.width, m_info.height};
            dispatch(translated_event);
            break;
        }
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
            if (event.window.windowID != m_window_id) {
                break;
            }

            m_framebuffer_width.store(static_cast<uint32_t>(std::max(event.window.data1, 0)), std::memory_order_relaxed);
            m_framebuffer_height.store(static_cast<uint32_t>(std::max(event.window.data2, 0)), std::memory_order_relaxed);
            break;
        }
        case SDL_EVENT_WINDOW_FOCUS_GAINED: {
            if (event.window.windowID == m_window_id) {
                core::WindowFocusEvent translated_event;
                dispatch(translated_event);
            }
            break;
        }
        case SDL_EVENT_WINDOW_FOCUS_LOST: {
            if (event.window.windowID == m_window_id) {
                core::WindowLostFocusEvent translated_event;
                dispatch(translated_event);
            }
            break;
        }
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            if (event.key.windowID != m_window_id) {
                break;
            }

            const core::KeyCode key = fromSDLScancode(event.key.scancode);
            if (key == core::KeyCode::None) {
                break;
            }

            if (event.type == SDL_EVENT_KEY_DOWN) {
                core::KeyPressedEvent translated_event{key, event.key.repeat};
                dispatch(translated_event);
            } else {
                core::KeyReleasedEvent translated_event{key};
                dispatch(translated_event);
            }
            break;
        }
        case SDL_EVENT_TEXT_INPUT: {
            if (event.text.windowID != m_window_id || event.text.text == nullptr) {
                break;
            }

            const char* text = event.text.text;
            while (const Uint32 codepoint = SDL_StepUTF8(&text, nullptr)) {
                core::KeyTypedEvent translated_event{codepoint};
                dispatch(translated_event);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            if (event.motion.windowID != m_window_id) {
                break;
            }

            core::Input::recordMouseMoved(event.motion.x, event.motion.y);
            core::MouseMovedEvent translated_event{event.motion.x, event.motion.y};
            dispatch(translated_event);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (event.button.windowID != m_window_id) {
                break;
            }

            const core::MouseCode button = fromSDLMouseButton(event.button.button);
            if (button == core::MouseCode::None) {
                break;
            }

            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                core::MouseButtonPressedEvent translated_event{button};
                dispatch(translated_event);
            } else {
                core::MouseButtonReleasedEvent translated_event{button};
                dispatch(translated_event);
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            if (event.wheel.windowID != m_window_id) {
                break;
            }

            const float direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
            const float x = event.wheel.x * direction;
            const float y = event.wheel.y * direction;
            core::Input::recordMouseScrolled(x, y);
            core::MouseScrolledEvent translated_event{x, y};
            dispatch(translated_event);
            break;
        }
        default:
            break;
    }
}

} // namespace arti::platform
