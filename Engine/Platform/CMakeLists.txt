add_library(NovaPlatform STATIC
    src/Window.cpp
)

target_include_directories(NovaPlatform PUBLIC include)
target_link_libraries(NovaPlatform PUBLIC NovaCore)
