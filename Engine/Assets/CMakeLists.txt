add_library(NovaAssets STATIC
    src/ObjLoader.cpp
    src/MeshCache.cpp
)

target_include_directories(NovaAssets PUBLIC include)
target_link_libraries(NovaAssets PUBLIC NovaCore NovaMath NovaRenderer)
