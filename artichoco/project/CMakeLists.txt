add_library(artichoco_project STATIC stub.cpp)
add_library(ArtiChoco::Project ALIAS artichoco_project)
artichoco_configure_library(artichoco_project)
