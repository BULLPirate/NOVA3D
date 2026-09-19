include(FetchContent)
FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(cgltf)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

add_library(NovaAssets STATIC
    src/ObjLoader.cpp
    src/GltfLoader.cpp
    src/MeshLoader.cpp
    src/MeshCache.cpp
    src/PngLoader.cpp
    src/TextureCache.cpp
    src/AssetRegistry.cpp
)

target_include_directories(NovaAssets PUBLIC include)
target_include_directories(NovaAssets PRIVATE ${cgltf_SOURCE_DIR} ${stb_SOURCE_DIR})
target_link_libraries(NovaAssets PUBLIC NovaCore NovaMath NovaRenderer)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_custom_command(
        OUTPUT
            ${CMAKE_SOURCE_DIR}/Assets/Characters/knight_armed.obj
            ${CMAKE_SOURCE_DIR}/Assets/Characters/bandit.obj
            ${CMAKE_SOURCE_DIR}/Assets/Models/pyramid.obj
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Tools/generate_knight_content.py --engine-only
        DEPENDS ${CMAKE_SOURCE_DIR}/Tools/generate_knight_content.py
        COMMENT "Generate NOVA3D character meshes and atlases"
        VERBATIM
    )
    add_custom_target(NovaGeneratedAssets DEPENDS
        ${CMAKE_SOURCE_DIR}/Assets/Characters/knight_armed.obj
        ${CMAKE_SOURCE_DIR}/Assets/Characters/bandit.obj
        ${CMAKE_SOURCE_DIR}/Assets/Models/pyramid.obj
    )
    add_dependencies(NovaAssets NovaGeneratedAssets)
endif()
