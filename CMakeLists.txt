cmake_minimum_required(VERSION 3.25)

project(Nova3D
    VERSION 0.1.0
    DESCRIPTION "NOVA3D — 3D-first game engine"
    LANGUAGES CXX C OBJCXX
)

# ── C++20 ──────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler warnings ──────────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC)
    add_compile_options(/W4)
endif()

# ── Build type defaults to Debug ───────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ── Output directories ─────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ── Global include path for Nova headers ───────────────────────────────────────
# Публичные заголовки лежат в Engine/<Module>/include/
# Подключение: #include <Nova/Core/Log.h>
include_directories(
    ${CMAKE_SOURCE_DIR}/Engine/Core/include
    ${CMAKE_SOURCE_DIR}/Engine/Platform/include
    ${CMAKE_SOURCE_DIR}/Engine/Math/include
)

# ── Subdirectories ─────────────────────────────────────────────────────────────
add_subdirectory(Engine)
add_subdirectory(Runtime)

option(NOVA_BUILD_TESTS "Build unit tests" ON)
if(NOVA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
