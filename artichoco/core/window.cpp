#include "window.h"

#include <memory>

namespace arti::core {
namespace {

class HeadlessWindow final : public Window {
public:
    explicit HeadlessWindow(const WindowCreateInfo& info)
        : m_width(info.width),
          m_height(info.height)
    {}

    void init() override {}

    void onUpdate() override
    {
        m_should_close = true;
    }

    void setEventCallback(const EventCallbackFn& callback) override
    {
        m_event_callback = callback;
    }

    bool shouldClose() override
    {
        return m_should_close;
    }

    uint32_t getWidth() const override
    {
        return m_width;
    }

    uint32_t getHeight() const override
    {
        return m_height;
    }

    uint32_t getFramebufferWidth() const override
    {
        return m_width;
    }

    uint32_t getFramebufferHeight() const override
    {
        return m_height;
    }

    void resize(uint32_t width, uint32_t height) override
    {
        m_width = width;
        m_height = height;
    }

private:
    uint32_t m_width{0};
    uint32_t m_height{0};
    bool m_should_close{false};
    EventCallbackFn m_event_callback;
};

} // namespace

std::unique_ptr<Window> createHeadlessWindow(const WindowCreateInfo& info)
{
    return std::make_unique<HeadlessWindow>(info);
}

} // namespace arti::core
