#pragma once

#include "asset_database.h"
#include "asset_loader.h"

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace arti::asset {

// AssetManager is the synchronous runtime loading and cache entry point.
class AssetManager final {
public:
    explicit AssetManager(AssetDatabase& database) noexcept
            : m_database(database) {}

    [[nodiscard]] bool registerLoader(std::unique_ptr<AssetLoader> loader);
    [[nodiscard]] bool unregisterLoader(const AssetType& type);

    [[nodiscard]] std::shared_ptr<Asset> load(core::UUID handle);

    template<HasAssetTraits T>
    [[nodiscard]] std::shared_ptr<T> load(core::UUID handle) {
        std::shared_ptr<Asset> loaded = load(handle);
        return std::dynamic_pointer_cast<T>(std::move(loaded));
    }

    void unload(core::UUID handle) noexcept { m_cache.erase(handle); }
    void clear() noexcept { m_cache.clear(); }

    [[nodiscard]] size_t cachedCount() const noexcept { return m_cache.size(); }

private:
    AssetDatabase& m_database;
    std::unordered_map<AssetType, std::unique_ptr<AssetLoader>> m_loaders;
    std::unordered_map<core::UUID, std::weak_ptr<Asset>> m_cache;
};

} // namespace arti::asset
