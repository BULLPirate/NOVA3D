add_library(NovaRenderer STATIC
    src/MetalRenderer.mm
)

set_source_files_properties(src/MetalRenderer.mm PROPERTIES
    COMPILE_FLAGS "-fobjc-arc"
)

target_include_directories(NovaRenderer PUBLIC include)

target_link_libraries(NovaRenderer PUBLIC
    NovaCore
    NovaPlatform
    "-framework Metal"
    "-framework QuartzCore"
    "-framework Foundation"
)
