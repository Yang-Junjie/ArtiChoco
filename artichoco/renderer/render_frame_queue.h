#pragma once
#include "render_frame_data.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace arti::renderer {

class RenderFrameQueue {
public:
    explicit RenderFrameQueue(size_t slot_count);

    RenderFrameQueue(const RenderFrameQueue&) = delete;
    RenderFrameQueue& operator=(const RenderFrameQueue&) = delete;

    RenderFrameData& acquireWriteSlot();
    void publish();
    const RenderFrameData* waitForNext();
    void releaseReadSlot();
    void waitUntilDrained();
    void shutdown() noexcept;

    size_t slotCount() const noexcept;

private:
    std::vector<RenderFrameData> m_slots;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    uint64_t m_published{0};
    uint64_t m_consumed{0};
    bool m_shutdown{false};
};

} // namespace arti::renderer
