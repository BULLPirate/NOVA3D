#include <Nova/Plugins/BuiltinPlugins.h>
#include <Nova/Plugins/FileStorageProvider.h>
#include <Nova/Plugins/NullAIProvider.h>

#include <memory>

namespace Nova {

void RegisterBuiltinPlugins(PluginRegistry& registry,
                            ServiceHub& services,
                            const std::filesystem::path& storageRoot) {
    auto ai = std::make_shared<NullAIProvider>();
    services.AI = ai;
    registry.Register(std::make_unique<NullAIPlugin>(ai));

    auto storage = std::make_shared<FileStorageProvider>(storageRoot);
    services.Storage = storage;
    registry.Register(std::make_unique<FileStoragePlugin>(storage));
}

} // namespace Nova
