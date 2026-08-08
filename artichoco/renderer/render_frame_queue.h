#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace arti::renderer {

template <typename T>
class RenderFrameQueue {
    static_assert(std::is_default_constructible_v<T> && std::is_move_assignable_v<T>,
                  "A render frame queue element must be default-constructible and move-assignable.");

public:
    explicit RenderFrameQueue(size_t slot_count)
        : m_slots(slot_count)
    {
        if (slot_count == 0) {
            throw std::invalid_argument("A render frame queue requires at least one slot.");
        }
    }

    RenderFrameQueue(const RenderFrameQueue&) = delete;
    RenderFrameQueue& operator=(const RenderFrameQueue&) = delete;

    T& acquireWriteSlot()
    {
        std::unique_lock lock{m_mutex};
        m_condition.wait(lock, [this] { return m_shutdown || m_published - m_consumed < m_slots.size(); });
        if (m_shutdown) {
            throw std::runtime_error("The render frame queue is shut down.");
        }
        return m_slots[m_published % m_slots.size()];
    }

    void publish()
    {
        std::lock_guard lock{m_mutex};
        ++m_published;
        m_condition.notify_all();
    }

    const T* waitForNext()
    {
        std::unique_lock lock{m_mutex};
        m_condition.wait(lock, [this] { return m_shutdown || m_published > m_consumed; });
        if (m_shutdown && m_published == m_consumed) {
            return nullptr;
        }
        return &m_slots[m_consumed % m_slots.size()];
    }

    void releaseReadSlot()
    {
        std::lock_guard lock{m_mutex};
        ++m_consumed;
        m_condition.notify_all();
    }

    void waitUntilDrained()
    {
        std::unique_lock lock{m_mutex};
        m_condition.wait(lock, [this] { return m_shutdown || m_published == m_consumed; });
    }

    void shutdown() noexcept
    {
        std::lock_guard lock{m_mutex};
        m_shutdown = true;
        m_condition.notify_all();
    }

    size_t slotCount() const noexcept
    {
        return m_slots.size();
    }

private:
    std::vector<T> m_slots;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    uint64_t m_published{0};
    uint64_t m_consumed{0};
    bool m_shutdown{false};
};

} // namespace arti::renderer
