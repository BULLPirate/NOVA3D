add_library(NovaCore STATIC
    src/Log.cpp
    src/Input.cpp
    src/Guid.cpp
    src/Time.cpp
    src/FileSystem.cpp
    src/Events.cpp
)

target_include_directories(NovaCore PUBLIC include)
target_link_libraries(NovaCore PUBLIC spdlog::spdlog)
