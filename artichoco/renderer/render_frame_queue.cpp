#include "render_frame_queue.h"

#include <stdexcept>
#include <utility>

namespace arti::renderer {

RenderFrameQueue::RenderFrameQueue(size_t slot_count)
    : m_slots(slot_count)
{
    if (slot_count == 0) {
        throw std::invalid_argument("A render frame queue requires at least one slot.");
    }
}

RenderFrameData& RenderFrameQueue::acquireWriteSlot()
{
    std::unique_lock lock{m_mutex};
    m_condition.wait(lock, [this] { return m_shutdown || m_published - m_consumed < m_slots.size(); });
    if (m_shutdown) {
        throw std::runtime_error("The render frame queue is shut down.");
    }
    return m_slots[m_published % m_slots.size()];
}

void RenderFrameQueue::publish()
{
    std::lock_guard lock{m_mutex};
    ++m_published;
    m_condition.notify_all();
}

const RenderFrameData* RenderFrameQueue::waitForNext()
{
    std::unique_lock lock{m_mutex};
    m_condition.wait(lock, [this] { return m_shutdown || m_published > m_consumed; });
    if (m_shutdown && m_published == m_consumed) {
        return nullptr;
    }
    return &m_slots[m_consumed % m_slots.size()];
}

void RenderFrameQueue::releaseReadSlot()
{
    std::lock_guard lock{m_mutex};
    ++m_consumed;
    m_condition.notify_all();
}

void RenderFrameQueue::waitUntilDrained()
{
    std::unique_lock lock{m_mutex};
    m_condition.wait(lock, [this] { return m_shutdown || m_published == m_consumed; });
}

void RenderFrameQueue::shutdown() noexcept
{
    std::lock_guard lock{m_mutex};
    m_shutdown = true;
    m_condition.notify_all();
}

size_t RenderFrameQueue::slotCount() const noexcept
{
    return m_slots.size();
}

} // namespace arti::renderer
