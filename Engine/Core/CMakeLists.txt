add_library(NovaCore STATIC
    src/Log.cpp
)

target_include_directories(NovaCore PUBLIC include)

find_package(Threads REQUIRED)
target_link_libraries(NovaCore PUBLIC Threads::Threads)
