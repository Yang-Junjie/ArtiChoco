#include "input.h"

namespace arti::core {
namespace {
InputProvider* s_provider = nullptr;

glm::vec2 s_mouse_position{0.0f, 0.0f};
glm::vec2 s_mouse_delta{0.0f, 0.0f};
glm::vec2 s_mouse_scroll_offset{0.0f, 0.0f};
bool s_has_mouse_position = false;
CursorMode s_cursor_mode = CursorMode::Normal;
} // namespace

bool Input::isKeyPressed(KeyCode key)
{
    return s_provider != nullptr && s_provider->isKeyPressed(key);
}

bool Input::isMouseButtonPressed(MouseCode button)
{
    return s_provider != nullptr && s_provider->isMouseButtonPressed(button);
}

glm::vec2 Input::getMousePosition()
{
    if (s_provider != nullptr) {
        s_mouse_position = s_provider->getMousePosition();
    }

    return s_mouse_position;
}

glm::vec2 Input::getMouseDelta()
{
    return s_mouse_delta;
}

glm::vec2 Input::getMouseScrollOffset()
{
    return s_mouse_scroll_offset;
}

void Input::setCursorMode(CursorMode mode)
{
    s_cursor_mode = mode;
    if (s_provider != nullptr) {
        s_provider->setCursorMode(mode);
    }
}

CursorMode Input::getCursorMode()
{
    if (s_provider != nullptr) {
        s_cursor_mode = s_provider->getCursorMode();
    }

    return s_cursor_mode;
}

void Input::setMousePosition(float x, float y)
{
    s_mouse_position = {x, y};
    s_has_mouse_position = true;
    if (s_provider != nullptr) {
        s_provider->setMousePosition(x, y);
    }
}

void Input::setRawMouseMotion(bool enabled)
{
    if (s_provider != nullptr) {
        s_provider->setRawMouseMotion(enabled);
    }
}

float Input::getMouseX()
{
    return getMousePosition().x;
}

float Input::getMouseY()
{
    return getMousePosition().y;
}

float Input::getMouseDeltaX()
{
    return s_mouse_delta.x;
}

float Input::getMouseDeltaY()
{
    return s_mouse_delta.y;
}

float Input::getMouseScrollX()
{
    return s_mouse_scroll_offset.x;
}

float Input::getMouseScrollY()
{
    return s_mouse_scroll_offset.y;
}

void Input::resetFrameState()
{
    s_mouse_delta = {0.0f, 0.0f};
    s_mouse_scroll_offset = {0.0f, 0.0f};
}

void Input::recordMouseMoved(float x, float y)
{
    if (s_has_mouse_position) {
        s_mouse_delta += glm::vec2{x, y} - s_mouse_position;
    } else {
        s_has_mouse_position = true;
    }

    s_mouse_position = {x, y};
}

void Input::recordMouseScrolled(float x_offset, float y_offset)
{
    s_mouse_scroll_offset += glm::vec2{x_offset, y_offset};
}

void Input::setProvider(InputProvider* provider)
{
    s_provider = provider;
    if (s_provider != nullptr) {
        s_mouse_position = s_provider->getMousePosition();
        s_cursor_mode = s_provider->getCursorMode();
        s_has_mouse_position = true;
    }
}
} // namespace arti::core
