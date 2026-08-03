#pragma once
#include "layer.h"

namespace arti::test_app {
class TestAppLayer final : public core::Layer {
public:
    TestAppLayer();

    void onAttach() override;
    void onRender() override;
};
} // namespace arti::test_app
