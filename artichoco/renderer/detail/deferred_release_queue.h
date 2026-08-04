#pragma once

#include <cstddef>

#include <future>
#include <mutex>
#include <vector>

namespace arti::renderer::detail {

class DeferredReleaseQueue {
public:
    explicit DeferredReleaseQueue(size_t frame_slot_count);

    DeferredReleaseQueue(const DeferredReleaseQueue&) = delete;
    DeferredReleaseQueue& operator=(const DeferredReleaseQueue&) = delete;

    void defer(std::packaged_task<void()> release);
    void onFrameSlotSubmitted(size_t frame_slot_index);
    void onFrameSlotCompleted(size_t frame_slot_index);
    void shutdown();

private:
    struct PendingRelease {
        std::packaged_task<void()> release;
        std::vector<bool> waiting_for_frame_slots;
        size_t remaining_frame_slots{0};
    };

    std::mutex m_mutex;
    std::vector<bool> m_submitted_frame_slots;
    std::vector<PendingRelease> m_pending_releases;
    bool m_shutdown{false};
};

} // namespace arti::renderer::detail
