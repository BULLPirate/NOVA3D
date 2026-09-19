#include <Nova/Scene/Script.h>
#include <Nova/Scene/Gameplay.h>

#include <Nova/Audio/AudioEngine.h>
#include <Nova/Core/FileSystem.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace Nova {

namespace {

std::string Lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string Trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string UniqueName(const Scene& scene, const std::string& base, Entity except = {}) {
    auto taken = [&](const std::string& name) {
        const Entity found = scene.FindEntityByName(name);
        return found.IsValid() && found.Id != except.Id;
    };
    if (!taken(base)) {
        return base;
    }
    for (int i = 2; i < 1000; ++i) {
        const std::string candidate = base + " " + std::to_string(i);
        if (!taken(candidate)) {
            return candidate;
        }
    }
    return base + " copy";
}

void ApplyConfig(Scene& scene, EngineSettings& settings, const ScriptInstr& instr) {
    switch (instr.Op) {
    case ScriptOp::Gravity:
        scene.ForEachEntity([&](Entity entity) {
            if (scene.HasCharacterController(entity)) {
                scene.GetCharacterController(entity).Gravity = instr.Value;
            }
        });
        break;
    case ScriptOp::Speed:
        scene.ForEachEntity([&](Entity entity) {
            if (scene.HasCharacterController(entity)) {
                scene.GetCharacterController(entity).MoveSpeed = instr.Value;
            }
        });
        break;
    case ScriptOp::Jump:
        scene.ForEachEntity([&](Entity entity) {
            if (scene.HasCharacterController(entity)) {
                scene.GetCharacterController(entity).JumpSpeed = instr.Value;
            }
        });
        break;
    case ScriptOp::Clear:
        scene.Settings().ClearColor = instr.Color;
        break;
    case ScriptOp::Volume:
        settings.MasterVolume = std::clamp(instr.Value, 0.0f, 1.0f);
        AudioEngine::Get().SetMasterVolume(settings.MasterVolume);
        break;
    case ScriptOp::Bind: {
        KeyCode code;
        if (KeyCodeFromName(instr.SoundId, code)) {
            settings.Bindings[instr.Action] = code;
        }
        break;
    }
    default:
        break;
    }
}

void RunInstr(Scene& scene, EngineSettings& settings, const ScriptInstr& instr, const Input* input) {
    switch (instr.Op) {
    case ScriptOp::Play:
        if (settings.EnableAudio) {
            AudioEngine::Get().Play(instr.SoundId);
        }
        break;
    case ScriptOp::IfPlay:
        if (settings.EnableAudio && input && settings.IsActionPressed(*input, instr.Action)) {
            AudioEngine::Get().Play(instr.SoundId);
        }
        break;
    default:
        ApplyConfig(scene, settings, instr);
        break;
    }
}

bool ParseConfig(const std::string& a, const std::string& b, const std::string& c, const std::string& d,
                 const std::string& e, ScriptInstr& instr) {
    if (a == "gravity" && !b.empty()) {
        instr.Op = ScriptOp::Gravity;
        instr.Value = std::strtof(b.c_str(), nullptr);
        return true;
    }
    if (a == "speed" && !b.empty()) {
        instr.Op = ScriptOp::Speed;
        instr.Value = std::strtof(b.c_str(), nullptr);
        return true;
    }
    if ((a == "jump" || a == "jumpspeed") && !b.empty()) {
        instr.Op = ScriptOp::Jump;
        instr.Value = std::strtof(b.c_str(), nullptr);
        return true;
    }
    if (a == "clear" && !b.empty() && !c.empty() && !d.empty()) {
        instr.Op = ScriptOp::Clear;
        instr.Color = {std::strtof(b.c_str(), nullptr), std::strtof(c.c_str(), nullptr),
                       std::strtof(d.c_str(), nullptr)};
        return true;
    }
    if (a == "volume" && !b.empty()) {
        instr.Op = ScriptOp::Volume;
        instr.Value = std::strtof(b.c_str(), nullptr);
        return true;
    }
    if (a == "bind" && !b.empty() && !c.empty()) {
        instr.Op = ScriptOp::Bind;
        instr.Action = b;
        std::string key = c;
        if (!key.empty()) {
            key[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
        }
        if (key == "space") {
            key = "Space";
        }
        instr.SoundId = key;
        (void)e;
        return true;
    }
    return false;
}

} // namespace

std::string DefaultPlayerScript() {
    return "speed 6.2\n"
           "jump 6.5\n";
}

std::string DefaultContentScript() {
    return "# Empty world script. Uncomment to spawn from code:\n"
           "# character\n"
           "# enemy\n"
           "# ground\n";
}

std::string DefaultGameScript() {
    return "gravity 20\n"
           "speed 6.2\n"
           "jump 6.5\n";
}

bool CompileNovaScript(std::string_view source, CompiledScript& out, std::string& error) {
    out = {};
    std::istringstream in{std::string(source)};
    std::string line;
    enum class Block { None, Start, Update } block = Block::None;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        line = Trim(line);
        if (line.empty()) {
            continue;
        }
        const std::string low = Lower(line);
        if (low == "on start") {
            block = Block::Start;
            continue;
        }
        if (low == "on update") {
            block = Block::Update;
            continue;
        }
        if (low == "end") {
            block = Block::None;
            continue;
        }
        std::istringstream tokens(low);
        std::string a, b, c, d, e;
        tokens >> a >> b >> c >> d >> e;
        ScriptInstr instr;
        bool ok = false;
        if (a == "play" && !b.empty()) {
            instr.Op = ScriptOp::Play;
            instr.SoundId = b;
            ok = true;
        } else if (a == "if" && !b.empty() && c == "play" && !d.empty()) {
            instr.Op = ScriptOp::IfPlay;
            instr.Action = b;
            instr.SoundId = d;
            ok = true;
        } else {
            ok = ParseConfig(a, b, c, d, e, instr);
        }
        if (!ok) {
            error = "line " + std::to_string(lineNo) + ": unknown command";
            return false;
        }
        if (block == Block::Update) {
            out.OnUpdate.push_back(std::move(instr));
        } else {
            out.OnStart.push_back(std::move(instr));
        }
    }
    return true;
}

void TickScripts(Scene& scene, const Input* input, EngineSettings& settings,
                 const std::filesystem::path& projectRoot, bool playJustStarted) {
    if (playJustStarted && !projectRoot.empty()) {
        const FileIOResult game = ReadTextFile(projectRoot / "Assets" / "Scripts" / "game.ns");
        if (game.Ok) {
            CompiledScript compiled;
            std::string error;
            if (CompileNovaScript(game.Text, compiled, error)) {
                for (const ScriptInstr& instr : compiled.OnStart) {
                    RunInstr(scene, settings, instr, input);
                }
            }
        }
    }

    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasScript(entity)) {
            return;
        }
        ScriptComponent& script = scene.GetScript(entity);
        if (script.Source.empty() && !script.AssetPath.empty() && !projectRoot.empty()) {
            const FileIOResult read = ReadTextFile(projectRoot / script.AssetPath);
            if (read.Ok) {
                script.Source = read.Text;
            }
        }
        CompiledScript compiled;
        std::string error;
        if (!CompileNovaScript(script.Source, compiled, error)) {
            return;
        }
        if (playJustStarted || !script.RanStart) {
            for (const ScriptInstr& instr : compiled.OnStart) {
                RunInstr(scene, settings, instr, input);
            }
            script.RanStart = true;
        }
        if (!playJustStarted) {
            for (const ScriptInstr& instr : compiled.OnUpdate) {
                RunInstr(scene, settings, instr, input);
            }
        }
    });

    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasAudioSource(entity)) {
            return;
        }
        AudioSourceComponent& src = scene.GetAudioSource(entity);
        if ((playJustStarted || !src.Started) && src.PlayOnStart && !src.SoundId.empty() &&
            settings.EnableAudio) {
            AudioEngine::Get().Play(src.SoundId);
            src.Started = true;
        }
    });
}

bool RunContentScript(Scene& scene, std::string_view source, std::string& error, Entity& lastSpawned) {
    lastSpawned = {};
    std::istringstream in{std::string(source)};
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        line = Trim(line);
        if (line.empty()) {
            continue;
        }
        std::istringstream tokens(line);
        std::string cmd;
        tokens >> cmd;
        const std::string low = Lower(cmd);
        std::string arg;
        tokens >> arg;
        if (low == "character" || low == "player") {
            lastSpawned = SpawnPlayer(scene, arg);
            scene.SetName(lastSpawned, UniqueName(scene, "Player", lastSpawned));
        } else if (low == "enemy" || low == "bandit") {
            lastSpawned = SpawnEnemy(scene, {4.0f, 0.0f, 3.0f}, arg);
        } else if (low == "item") {
            lastSpawned = SpawnImportedMesh(scene, UniqueName(scene, "Item"), arg);
        } else if (low == "map" || low == "environment") {
            lastSpawned = SpawnImportedMesh(scene, UniqueName(scene, "Map"), arg);
        } else if (low == "ground") {
            lastSpawned = SpawnGround(scene);
            scene.SetName(lastSpawned, UniqueName(scene, "Ground", lastSpawned));
        } else if (low == "cube" || low == "mesh") {
            lastSpawned = scene.CreateEntity(UniqueName(scene, "Mesh"));
            scene.AddMeshRenderer(lastSpawned);
        } else if (low == "sphere") {
            lastSpawned = scene.CreateEntity(UniqueName(scene, "Sphere"));
            MeshRendererComponent mesh;
            mesh.Primitive = MeshPrimitive::UnitSphere;
            scene.AddMeshRenderer(lastSpawned, mesh);
        } else if (low == "plane") {
            lastSpawned = scene.CreateEntity(UniqueName(scene, "Plane"));
            MeshRendererComponent mesh;
            mesh.Primitive = MeshPrimitive::UnitPlane;
            scene.AddMeshRenderer(lastSpawned, mesh);
        } else {
            error = "line " + std::to_string(lineNo) + ": unknown content command";
            return false;
        }
    }
    return true;
}

} // namespace Nova
