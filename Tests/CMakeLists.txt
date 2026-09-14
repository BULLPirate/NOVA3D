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
)

target_link_libraries(NovaTests PRIVATE
    NovaCore
    NovaMath
    NovaRenderer
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(NovaTests)
