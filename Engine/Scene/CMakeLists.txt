include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(NovaScene STATIC
    src/Scene.cpp
    src/SceneSerialization.cpp
    src/SceneRendererBridge.cpp
    src/SceneRuntime.cpp
    src/Project.cpp
)

target_include_directories(NovaScene PUBLIC include)
target_link_libraries(NovaScene PUBLIC NovaMath NovaRenderer NovaAssets nlohmann_json::nlohmann_json)
