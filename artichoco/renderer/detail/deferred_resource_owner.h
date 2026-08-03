#pragma once

#include <future>
#include <memory>

namespace arti::renderer::detail {

class DeferredResourceOwner {
public:
    virtual ~DeferredResourceOwner() = default;

    virtual void deferRelease(std::packaged_task<void()> release) = 0;
};

using DeferredResourceOwnerPtr = std::shared_ptr<DeferredResourceOwner>;

} // namespace arti::renderer::detail
