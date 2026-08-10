#pragma once
#include "artichoco/core/timestep.h"

#include <algorithm>
#include <stdexcept>

namespace arti::core {

class FixedTimestepAccumulator {
public:
    explicit FixedTimestepAccumulator(float fixed_dt = 1.0f / 60.0f, float max_frame_time = 0.25f)
            : m_fixed_dt(fixed_dt),
              m_max_frame_time(max_frame_time) {
        if (fixed_dt <= 0.0f || max_frame_time < fixed_dt) {
            throw std::invalid_argument(
                    "A fixed timestep requires a positive step no larger than the frame clamp.");
        }
    }

    template<typename Callback>
    void tick(float real_delta, Callback&& fixed_step) {
        m_accumulator += std::min(real_delta, m_max_frame_time);
        while (m_accumulator >= m_fixed_dt) {
            fixed_step(m_fixed_dt);
            m_accumulator -= m_fixed_dt;
        }
    }

    float fixedDeltaTime() const noexcept { return m_fixed_dt; }

    float alpha() const noexcept { return m_accumulator / m_fixed_dt; }

private:
    float m_fixed_dt{ 1.0f / 60.0f };
    float m_max_frame_time{ 0.25f };
    float m_accumulator{ 0.0f };
};

} // namespace arti::core
