#pragma once

#include <memory>

namespace arti::renderer::detail {

class ResourceOwner {
public:
    virtual ~ResourceOwner() = default;
};

using ResourceOwnerPtr = std::shared_ptr<ResourceOwner>;

} // namespace arti::renderer::detail
