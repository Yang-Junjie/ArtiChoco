#include "deferred_release_queue.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace arti::renderer::detail {

DeferredReleaseQueue::DeferredReleaseQueue(size_t frame_slot_count)
    : m_submitted_frame_slots(frame_slot_count, false)
{
    if (frame_slot_count == 0) {
        throw std::invalid_argument("DeferredReleaseQueue requires at least one frame slot.");
    }
}

void DeferredReleaseQueue::defer(std::packaged_task<void()> release)
{
    {
        std::scoped_lock lock{m_mutex};
        if (!m_shutdown) {
            PendingRelease pending;
            pending.release = std::move(release);
            pending.waiting_for_frame_slots = m_submitted_frame_slots;
            pending.remaining_frame_slots = static_cast<size_t>(std::ranges::count(m_submitted_frame_slots, true));
            m_pending_releases.push_back(std::move(pending));
            return;
        }
    }

    release();
}

void DeferredReleaseQueue::onFrameSlotSubmitted(size_t frame_slot_index)
{
    std::scoped_lock lock{m_mutex};
    m_submitted_frame_slots.at(frame_slot_index) = true;
}

void DeferredReleaseQueue::onFrameSlotCompleted(size_t frame_slot_index)
{
    std::vector<std::packaged_task<void()>> ready;
    {
        std::scoped_lock lock{m_mutex};
        m_submitted_frame_slots.at(frame_slot_index) = false;
        for (auto iterator = m_pending_releases.begin(); iterator != m_pending_releases.end();) {
            if (iterator->waiting_for_frame_slots.at(frame_slot_index)) {
                iterator->waiting_for_frame_slots[frame_slot_index] = false;
                --iterator->remaining_frame_slots;
            }
            if (iterator->remaining_frame_slots == 0) {
                ready.push_back(std::move(iterator->release));
                iterator = m_pending_releases.erase(iterator);
            } else {
                ++iterator;
            }
        }
    }

    for (auto& release : ready) {
        release();
    }
}

void DeferredReleaseQueue::shutdown()
{
    std::vector<std::packaged_task<void()>> ready;
    {
        std::scoped_lock lock{m_mutex};
        if (m_shutdown) {
            return;
        }

        m_shutdown = true;
        std::ranges::fill(m_submitted_frame_slots, false);
        ready.reserve(m_pending_releases.size());
        for (auto& pending : m_pending_releases) {
            ready.push_back(std::move(pending.release));
        }
        m_pending_releases.clear();
    }

    for (auto& release : ready) {
        release();
    }
}

} // namespace arti::renderer::detail
