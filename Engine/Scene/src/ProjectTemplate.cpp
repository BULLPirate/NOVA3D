#include <Nova/Project/ProjectTemplate.h>
#include <Nova/Scene/Gameplay.h>
#include <Nova/Scene/SceneSerialization.h>
#include <Nova/Scene/Script.h>
#include <Nova/Core/FileSystem.h>

#include <system_error>
#include <vector>

namespace Nova {

namespace {

void CopyEngineAsset(const std::filesystem::path& engineRoot,
                     const std::filesystem::path& projectRoot, const std::filesystem::path& relative) {
    if (engineRoot.empty()) {
        return;
    }
    const std::filesystem::path src = engineRoot / relative;
    const std::filesystem::path dst = projectRoot / relative;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(src, ec)) {
        return;
    }
    std::filesystem::create_directories(dst.parent_path(), ec);
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
}

void CopyGameKit(const std::filesystem::path& engineRoot, const std::filesystem::path& projectRoot) {
    CopyEngineAsset(engineRoot, projectRoot, "Assets/Textures/ground.png");
    CopyEngineAsset(engineRoot, projectRoot, "Assets/Textures/stone.png");
    CopyEngineAsset(engineRoot, projectRoot, "Assets/Textures/checker.png");
    CopyEngineAsset(engineRoot, projectRoot, "Assets/Textures/characters.png");
    CopyEngineAsset(engineRoot, projectRoot, "Assets/Characters/knight_armed.obj");
    CopyEngineAsset(engineRoot, projectRoot, "Assets/Characters/bandit.obj");
    CopyEngineAsset(engineRoot, projectRoot, "Assets/Characters/tree.obj");
    CopyEngineAsset(engineRoot, projectRoot, "Assets/Characters/crate.obj");
}

void PaintKnightBanditsContent(Scene& scene) {
    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasMeshRenderer(entity)) {
            return;
        }
        MeshRendererComponent& mesh = scene.GetMeshRenderer(entity);
        const std::string& name = scene.GetName(entity);
        if (name == "Ground") {
            mesh.AlbedoTexturePath = "Assets/Textures/ground.png";
            mesh.UseAlbedoTexture = true;
        } else if (name.rfind("Wall", 0) == 0 || name.rfind("House", 0) == 0 || name == "Well" ||
                   name.rfind("Fence", 0) == 0 || name == "Road") {
            mesh.AlbedoTexturePath = "Assets/Textures/stone.png";
            mesh.UseAlbedoTexture = true;
        }
    });

    if (Entity player = scene.FindEntityByName("Player"); player.IsValid()) {
        SetEntityMeshAsset(scene, player, "Assets/Characters/knight_armed.obj",
                           "Assets/Textures/characters.png");
    }
    std::vector<Entity> bandits;
    std::vector<Entity> props;
    scene.ForEachEntity([&](Entity entity) {
        if (scene.HasCharacterController(entity) && scene.GetCharacterController(entity).Team == 1) {
            bandits.push_back(entity);
            return;
        }
        if (scene.GetParent(entity).IsValid()) {
            return;
        }
        const std::string& name = scene.GetName(entity);
        if (name.rfind("Tree", 0) == 0 || name.rfind("Crate", 0) == 0) {
            props.push_back(entity);
        }
    });
    for (Entity entity : bandits) {
        SetEntityMeshAsset(scene, entity, "Assets/Characters/bandit.obj",
                           "Assets/Textures/characters.png");
    }
    for (Entity entity : props) {
        const std::string& name = scene.GetName(entity);
        if (name.rfind("Tree", 0) == 0) {
            SetEntityMeshAsset(scene, entity, "Assets/Characters/tree.obj", {});
        } else {
            SetEntityMeshAsset(scene, entity, "Assets/Characters/crate.obj",
                               "Assets/Textures/stone.png");
        }
        scene.GetTransform(entity).Position.y = 0.0f;
    }
}

bool IsKnightBanditsScene(const Scene& scene) {
    if (!scene.Settings().EnableWaves || !scene.FindEntityByName("Player").IsValid() ||
        !scene.FindEntityByName("House A").IsValid()) {
        return false;
    }
    bool bandit = false;
    scene.ForEachEntity([&](Entity entity) {
        if (scene.HasCharacterController(entity) && scene.GetCharacterController(entity).Team == 1) {
            bandit = true;
        }
    });
    return bandit;
}

bool KnightUsesCharacterMeshes(const Scene& scene) {
    const Entity player = scene.FindEntityByName("Player");
    return player.IsValid() && scene.HasMeshRenderer(player) &&
           !scene.GetMeshRenderer(player).AssetPath.empty();
}

std::string TemplateReadme(ProjectTemplateKind kind, const std::string& name) {
    if (kind == ProjectTemplateKind::KnightBandits) {
        return "# Knight Bandits\n\n"
               "Separate game project. WASD walk, mouse look, F/LMB attack, Shift sprint, "
               "R stand up, F8 save, F9 load. Five waves.\n";
    }
    if (kind == ProjectTemplateKind::ThirdPerson) {
        return "# " + name + "\n\nEmpty third-person sandbox: ground, player, follow camera.\n";
    }
    return "# " + name + "\n\nEmpty NOVA3D project. Place objects in the editor, then Play.\n";
}

Scene BuildTemplateScene(ProjectTemplateKind kind) {
    if (kind == ProjectTemplateKind::KnightBandits) {
        Scene scene = Scene::CreatePlayableLevel();
        scene.Settings().EnableWaves = true;
        PaintKnightBanditsContent(scene);
        return scene;
    }
    if (kind == ProjectTemplateKind::ThirdPerson) {
        Scene scene = Scene::CreateSandboxLevel();
        scene.Settings().EnableWaves = false;
        return scene;
    }
    Scene scene = Scene::CreateEmptyLevel();
    scene.Settings().EnableWaves = false;
    return scene;
}

ProjectIOResult WriteTemplateScene(const std::filesystem::path& projectRoot, const Scene& scene) {
    ProjectIOResult result;
    const std::filesystem::path scenePath = projectRoot / "Assets" / "Scenes" / "main.scene.json";
    if (SceneIOResult save = SaveSceneToFile(scene, scenePath); !save.Ok) {
        result.Error = save.Error;
        return result;
    }
    result.Ok = true;
    return result;
}

} // namespace

ProjectIOResult CreateGameProject(const std::filesystem::path& projectRoot, const std::string& name,
                                  ProjectTemplateKind kind, const std::filesystem::path& engineRoot) {
    ProjectIOResult result = InitializeNewProject(projectRoot, name);
    if (!result.Ok) {
        return result;
    }

    CopyGameKit(engineRoot, projectRoot);
    const Scene scene = BuildTemplateScene(kind);
    if (ProjectIOResult written = WriteTemplateScene(projectRoot, scene); !written.Ok) {
        return written;
    }

    std::string error;
    CreateDirectories(projectRoot / "Saves", error);
    CreateDirectories(projectRoot / "Assets" / "UI", error);
    WriteTextFile(projectRoot / "README.md", TemplateReadme(kind, name));
    WriteTextFile(projectRoot / "Assets" / "UI" / "icon.txt", name.empty() ? "game" : name);
    WriteTextFile(projectRoot / "Assets" / "Scripts" / "world.ns",
                  kind == ProjectTemplateKind::KnightBandits
                      ? "# Knight Bandits world\n# Edit in the Content tab.\n"
                      : DefaultContentScript());
    return result;
}

ProjectIOResult EnsureGameProject(const std::filesystem::path& projectRoot, const std::string& name,
                                  ProjectTemplateKind kind, const std::filesystem::path& engineRoot) {
    if (!IsNovaProjectRoot(projectRoot)) {
        return CreateGameProject(projectRoot, name, kind, engineRoot);
    }

    CopyGameKit(engineRoot, projectRoot);
    if (kind != ProjectTemplateKind::KnightBandits) {
        ProjectIOResult ok;
        ok.Ok = true;
        return ok;
    }

    const std::filesystem::path scenePath = projectRoot / "Assets" / "Scenes" / "main.scene.json";
    Scene scene;
    const bool loaded = LoadSceneFromFile(scenePath, scene).Ok;
    if (loaded && IsKnightBanditsScene(scene) && KnightUsesCharacterMeshes(scene)) {
        ProjectIOResult ok;
        ok.Ok = true;
        return ok;
    }

    scene = BuildTemplateScene(kind);
    return WriteTemplateScene(projectRoot, scene);
}

} // namespace Nova
