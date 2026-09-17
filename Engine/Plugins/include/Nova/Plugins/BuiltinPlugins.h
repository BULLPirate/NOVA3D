#pragma once

#include <Nova/Plugins/PluginRegistry.h>
#include <Nova/Plugins/ServiceHub.h>

#include <filesystem>

namespace Nova {

/// Registers the built-in null AI and file storage providers. No vendor SDKs.
void RegisterBuiltinPlugins(PluginRegistry& registry,
                            ServiceHub& services,
                            const std::filesystem::path& storageRoot);

} // namespace Nova
