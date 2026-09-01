add_library(artichoco_asset STATIC
    asset_catalog.cpp
    asset_importer.cpp
    asset_log.cpp
    asset_manager.cpp
    asset_manifest.cpp
    asset_metadata.cpp
    asset_settings.cpp
    asset_storage.cpp
)
add_library(ArtiChoco::Asset ALIAS artichoco_asset)
artichoco_configure_library(artichoco_asset)
target_link_libraries(artichoco_asset
    PUBLIC ArtiChoco::Core
    PRIVATE yaml-cpp::yaml-cpp
)
