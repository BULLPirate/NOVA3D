add_executable(Nova3D MACOSX_BUNDLE src/main.cpp)

set_target_properties(Nova3D PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Runtime/Info.plist
    BUILD_RPATH "@executable_path/../Frameworks;${CMAKE_BINARY_DIR}/lib"
    INSTALL_RPATH "@executable_path/../Frameworks"
)

target_link_libraries(Nova3D PRIVATE NovaCore NovaPlatform NovaMath NovaRenderer)

add_custom_command(TARGET Nova3D POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:Nova3D>/../Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3>"
        "$<TARGET_FILE_DIR:Nova3D>/../Frameworks/"
)
