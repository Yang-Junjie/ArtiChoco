#include "application.h"
#include "test_app_layer.h"

namespace arti::test_app {

TestAppLayer::TestAppLayer()
    : Layer("TestAppLayer")
{}

void TestAppLayer::onAttach()
{
    core::Application::get().getLogChannel().info("hello world");
}

void TestAppLayer::onRender() {}
} // namespace arti::test_app
