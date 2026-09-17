#pragma once

#include <Nova/Plugins/IPlugin.h>
#include <Nova/Plugins/IStorageProvider.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace Nova {

class FileStorageProvider : public IStorageProvider {
public:
    explicit FileStorageProvider(std::filesystem::path root);

    std::string Id() const override { return "nova.storage.file"; }
    StorageResult Save(std::string_view key, std::string_view data) override;
    StorageResult Load(std::string_view key) override;
    bool Exists(std::string_view key) const override;
    StorageResult Remove(std::string_view key) override;

    const std::filesystem::path& Root() const { return m_Root; }

private:
    bool ResolveKey(std::string_view key, std::filesystem::path& out, std::string& error) const;

    std::filesystem::path m_Root;
};

class FileStoragePlugin : public IPlugin {
public:
    explicit FileStoragePlugin(std::shared_ptr<FileStorageProvider> provider);

    PluginInfo Info() const override;
    void SetEnabled(bool enabled) override { m_Enabled = enabled; }
    bool IsEnabled() const override { return m_Enabled; }

private:
    std::shared_ptr<FileStorageProvider> m_Provider;
    bool m_Enabled = true;
};

} // namespace Nova
