#pragma once

#include "key_codes.h"
#include "mouse_codes.h"

#include <glm/glm.hpp>

namespace arti::core {

class InputProvider {
public:
    virtual ~InputProvider() = default;

    virtual bool isKeyPressed(KeyCode key) const = 0;
    virtual bool isMouseButtonPressed(MouseCode button) const = 0;
    virtual glm::vec2 getMousePosition() const = 0;
    virtual CursorMode getCursorMode() const = 0;
    virtual void setCursorMode(CursorMode mode) = 0;
    virtual void setMousePosition(float x, float y) = 0;
    virtual void setRawMouseMotion(bool enabled) = 0;
};

class Input {
public:
    static bool isKeyPressed(KeyCode key);

    static bool isMouseButtonPressed(MouseCode button);

    static glm::vec2 getMousePosition();

    static glm::vec2 getMouseDelta();

    static glm::vec2 getMouseScrollOffset();

    static void setCursorMode(CursorMode mode);

    static CursorMode getCursorMode();

    static void setMousePosition(float x, float y);
    static void setRawMouseMotion(bool enabled);

    static float getMouseX();

    static float getMouseY();

    static float getMouseDeltaX();

    static float getMouseDeltaY();

    static float getMouseScrollX();

    static float getMouseScrollY();

    static void resetFrameState();
    static void recordMouseMoved(float x, float y);
    static void recordMouseScrolled(float x_offset, float y_offset);

    static void setProvider(InputProvider* provider);
};
} // namespace arti::core
