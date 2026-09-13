add_library(NovaCore STATIC
    src/Log.cpp
)

target_include_directories(NovaCore PUBLIC include)
target_link_libraries(NovaCore PUBLIC spdlog::spdlog)
