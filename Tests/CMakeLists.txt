FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(NovaTests
    LogTest.cpp
    MathTest.cpp
    InputTest.cpp
    RendererTest.cpp
    CameraTest.cpp
    MeshTest.cpp
    TextureTest.cpp
    LightingTest.cpp
    SceneTest.cpp
    SceneSerializationTest.cpp
    ProjectTest.cpp
    SceneRuntimeTest.cpp
    ObjLoaderTest.cpp
    GltfLoaderTest.cpp
    PngLoaderTest.cpp
    ViewportManipulatorTest.cpp
    ScenePickingTest.cpp
    EditorHistoryTest.cpp
)

target_compile_definitions(NovaTests PRIVATE "NOVA_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")

target_sources(NovaTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src/ViewportManipulator.cpp
)

target_include_directories(NovaTests PRIVATE ${CMAKE_SOURCE_DIR}/Editor/include)

target_link_libraries(NovaTests PRIVATE
    NovaCore
    NovaMath
    NovaRenderer
    NovaScene
    NovaAssets
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(NovaTests)
