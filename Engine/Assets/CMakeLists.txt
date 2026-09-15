include(FetchContent)
FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(cgltf)

add_library(NovaAssets STATIC
    src/ObjLoader.cpp
    src/GltfLoader.cpp
    src/MeshLoader.cpp
    src/MeshCache.cpp
)

target_include_directories(NovaAssets PUBLIC include)
target_include_directories(NovaAssets PRIVATE ${cgltf_SOURCE_DIR})
target_link_libraries(NovaAssets PUBLIC NovaCore NovaMath NovaRenderer)
