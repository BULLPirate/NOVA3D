#include "GameLauncher.h"

#include <Nova/Project/Project.h>
#include <Nova/Core/Log.h>

#import <Foundation/Foundation.h>

#include <cstdlib>
#include <vector>

#ifndef NOVA_BUILD_DIR
#define NOVA_BUILD_DIR "build"
#endif

namespace Nova::Editor {

namespace {

std::filesystem::path ExecutablePath() {
    @autoreleasepool {
        NSBundle* bundle = [NSBundle mainBundle];
        NSString* path = [bundle executablePath];
        if (path) {
            return std::filesystem::path([path UTF8String]);
        }
    }
    return {};
}

std::string ShellQuote(const std::filesystem::path& path) {
    const std::string s = path.string();
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += "'";
    return out;
}

} // namespace

std::filesystem::path ResolveNova3DAppBundle() {
    const std::filesystem::path exe = ExecutablePath();
    if (!exe.empty()) {
        const std::filesystem::path sibling =
            exe.parent_path().parent_path().parent_path() / "Nova3D.app";
        if (std::filesystem::exists(sibling)) {
            return sibling;
        }
    }

    const std::filesystem::path fromBuild =
        std::filesystem::path(NOVA_BUILD_DIR) / "bin" / "Nova3D.app";
    if (std::filesystem::exists(fromBuild)) {
        return std::filesystem::weakly_canonical(fromBuild);
    }

    return {};
}

bool LaunchGame(const Nova::ProjectDescriptor& project, const std::filesystem::path& scenePath) {
    if (project.Root.empty() || !Nova::IsNovaProjectRoot(project.Root)) {
        NOVA_LOG_ERROR("Cannot launch game: no project folder is open");
        return false;
    }
    const std::filesystem::path app = ResolveNova3DAppBundle();
    if (app.empty() || !std::filesystem::exists(app)) {
        NOVA_LOG_ERROR("Nova3D.app not found — build with: cmake --build build --target Nova3D");
        return false;
    }

    std::string cmd = "open -n " + ShellQuote(app) + " --args --project " +
                      ShellQuote(std::filesystem::absolute(project.Root));
    if (!scenePath.empty()) {
        cmd += " --scene " + ShellQuote(std::filesystem::absolute(scenePath));
    }
    NOVA_LOG_INFO("Launching game: {}", cmd);
    const int code = std::system(cmd.c_str());
    if (code != 0) {
        NOVA_LOG_ERROR("Failed to launch Nova3D (exit {})", code);
        return false;
    }
    return true;
}

} // namespace Nova::Editor
