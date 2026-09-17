add_library(NovaPlugins STATIC
    src/PluginRegistry.cpp
    src/NullAIProvider.cpp
    src/FileStorageProvider.cpp
    src/BuiltinPlugins.cpp
)

target_include_directories(NovaPlugins PUBLIC include)
target_link_libraries(NovaPlugins PUBLIC NovaCore)
