add_library(artichoco_scene STATIC stub.cpp)
add_library(ArtiChoco::Scene ALIAS artichoco_scene)
artichoco_configure_library(artichoco_scene)
target_link_libraries(artichoco_scene PUBLIC ArtiSDK::GLM)
