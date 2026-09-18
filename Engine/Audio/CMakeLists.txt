add_library(NovaAudio STATIC
    src/AudioEngine.cpp
)

target_include_directories(NovaAudio PUBLIC include)
target_link_libraries(NovaAudio PUBLIC NovaCore SDL3::SDL3)
