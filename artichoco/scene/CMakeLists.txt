add_library(artichoco_scene STATIC
    scene.cpp
    scene_log.cpp
    scene_serialization_registry.cpp
    scene_serializer.cpp
)
add_library(ArtiChoco::Scene ALIAS artichoco_scene)
artichoco_configure_library(artichoco_scene)
target_link_libraries(artichoco_scene
    PUBLIC
        ArtiChoco::Core
        ArtiSDK::GLM
        EnTT::EnTT
        yaml-cpp::yaml-cpp
)
