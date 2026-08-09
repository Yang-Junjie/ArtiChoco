#include "asset_manager.h"

#include "asset_log.h"

#include <exception>

namespace arti::asset {

bool AssetManager::registerLoader(std::unique_ptr<AssetLoader> loader) {
    if (!loader) {
        getLogChannel().error("Cannot register a null AssetLoader");
        return false;
    }

    AssetType type;
    try {
        type = loader->getType();
    } catch (const std::exception& exception) {
        getLogChannel().error("AssetLoader failed while declaring its type: {}", exception.what());
        return false;
    } catch (...) {
        getLogChannel().error("AssetLoader threw an unknown exception while declaring its type");
        return false;
    }
    if (!type.isValid()) {
        getLogChannel().error("Cannot register an AssetLoader with an empty AssetType");
        return false;
    }
    if (m_loaders.contains(type)) {
        getLogChannel().error("An AssetLoader is already registered for '{}'", type.name());
        return false;
    }

    getLogChannel().info("Registered AssetLoader for '{}'", type.name());
    return m_loaders.emplace(type, std::move(loader)).second;
}

bool AssetManager::unregisterLoader(const AssetType& type) {
    if (m_loaders.erase(type) == 0) {
        getLogChannel().warn("No AssetLoader was registered for '{}'", type.name());
        return false;
    }
    getLogChannel().info("Unregistered AssetLoader for '{}'", type.name());
    return true;
}

std::shared_ptr<Asset> AssetManager::load(core::UUID handle) {
    if (!handle.isValid()) {
        getLogChannel().error("Cannot load an Asset with an invalid UUID");
        return {};
    }

    if (const auto cached = m_cache.find(handle); cached != m_cache.end()) {
        if (std::shared_ptr<Asset> asset = cached->second.lock()) {
            getLogChannel().debug("Using cached Asset {}", handle.toString());
            return asset;
        }
        m_cache.erase(cached);
    }

    const auto metadata = m_database.find(handle);
    if (!metadata) {
        getLogChannel().error("Asset {} is not present in the AssetDatabase", handle.toString());
        return {};
    }

    const auto loader = m_loaders.find(metadata->type);
    if (loader == m_loaders.end()) {
        getLogChannel().error("No AssetLoader is registered for '{}'", metadata->type.name());
        return {};
    }

    const auto artifact_file = m_database.resolveArtifactPath(metadata->artifact_path);
    if (!artifact_file) {
        getLogChannel().error("Asset {} contains an invalid Artifact path", handle.toString());
        return {};
    }

    std::shared_ptr<Asset> loaded;
    try {
        loaded = loader->second->load({
            .metadata = *metadata,
            .artifact_file = *artifact_file,
        });
    } catch (const std::exception& exception) {
        getLogChannel().error("AssetLoader for '{}' threw an exception: {}", metadata->type.name(),
                exception.what());
        return {};
    } catch (...) {
        getLogChannel().error("AssetLoader for '{}' threw an unknown exception",
                metadata->type.name());
        return {};
    }

    if (!loaded) {
        getLogChannel().error("AssetLoader failed to load {}", handle.toString());
        return {};
    }
    try {
        if (loaded->getHandle() != handle || loaded->getType() != metadata->type) {
            getLogChannel().error("AssetLoader returned an Asset with the wrong identity or type");
            return {};
        }
    } catch (const std::exception& exception) {
        getLogChannel().error("Loaded Asset failed identity validation: {}", exception.what());
        return {};
    } catch (...) {
        getLogChannel().error("Loaded Asset threw an unknown exception during validation");
        return {};
    }

    m_cache.insert_or_assign(handle, loaded);
    getLogChannel().info("Loaded Asset {} ({})", handle.toString(), metadata->type.name());
    return loaded;
}

} // namespace arti::asset
