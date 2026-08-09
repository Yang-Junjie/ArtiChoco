add_library(artichoco_asset STATIC
    asset_database.cpp
    asset_log.cpp
    asset_manager.cpp
    asset_metadata.cpp
)
add_library(ArtiChoco::Asset ALIAS artichoco_asset)
artichoco_configure_library(artichoco_asset)
target_link_libraries(artichoco_asset
    PUBLIC ArtiChoco::Core
    PRIVATE yaml-cpp::yaml-cpp
)
