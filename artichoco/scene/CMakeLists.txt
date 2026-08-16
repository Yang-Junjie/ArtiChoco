add_library(artichoco_scene STATIC
    scene.cpp
    scene_cloner.cpp
    scene_entity_storage.cpp
    scene_hierarchy.cpp
    scene_log.cpp
    scene_serialization_registry.cpp
    scene_serializer.cpp
    scene_system_manager.cpp
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
