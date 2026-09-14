include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})

add_library(NovaImGui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_metal.mm
)

target_include_directories(NovaImGui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

set_source_files_properties(${IMGUI_DIR}/backends/imgui_impl_metal.mm PROPERTIES
    COMPILE_FLAGS "-fobjc-arc"
)

target_link_libraries(NovaImGui PUBLIC SDL3::SDL3)

add_executable(NovaEditor MACOSX_BUNDLE src/main.mm)

set_source_files_properties(src/main.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")

set_target_properties(NovaEditor PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Runtime/Info.plist
    BUILD_RPATH "@executable_path/../Frameworks;${CMAKE_BINARY_DIR}/lib"
    INSTALL_RPATH "@executable_path/../Frameworks"
)

target_compile_definitions(NovaEditor PRIVATE "NOVA_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")

target_link_libraries(NovaEditor PRIVATE
    NovaCore
    NovaPlatform
    NovaMath
    NovaRenderer
    NovaScene
    NovaImGui
    "-framework Metal"
    "-framework QuartzCore"
    "-framework Foundation"
)

add_custom_command(TARGET NovaEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:NovaEditor>/../Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3>"
        "$<TARGET_FILE_DIR:NovaEditor>/../Frameworks/"
)
