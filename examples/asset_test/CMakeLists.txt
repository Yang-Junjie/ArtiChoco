add_executable(asset_test asset_test.cpp)
target_link_libraries(asset_test
    PRIVATE
        ArtiChoco::Asset
        ArtiChoco::Project
)
set_target_properties(asset_test PROPERTIES FOLDER "Examples")
