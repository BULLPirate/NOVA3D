add_library(NovaMath STATIC
    src/Vec.cpp
    src/Mat4.cpp
)

target_include_directories(NovaMath PUBLIC include)
