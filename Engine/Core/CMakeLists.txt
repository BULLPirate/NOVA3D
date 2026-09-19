add_library(NovaCore STATIC
    src/Log.cpp
    src/Input.cpp
    src/Guid.cpp
    src/Time.cpp
    src/FileSystem.cpp
    src/Events.cpp
    src/EngineSettings.cpp
)

target_include_directories(NovaCore PUBLIC include)
target_compile_definitions(NovaCore PRIVATE "NOVA_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")
target_link_libraries(NovaCore PUBLIC spdlog::spdlog)
