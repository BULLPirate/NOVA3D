add_library(NovaPlatform STATIC
    src/Window.cpp
    src/MacApp.mm
)

set_source_files_properties(src/MacApp.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")

target_include_directories(NovaPlatform PUBLIC include)
target_link_libraries(NovaPlatform PUBLIC
    NovaCore
    SDL3::SDL3
    "-framework AppKit"
    "-framework Foundation"
)
