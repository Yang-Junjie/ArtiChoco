add_library(artichoco_asset STATIC stub.cpp)
add_library(ArtiChoco::Asset ALIAS artichoco_asset)
artichoco_configure_library(artichoco_asset)
