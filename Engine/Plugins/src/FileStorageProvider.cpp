#include <Nova/Plugins/FileStorageProvider.h>
#include <Nova/Core/FileSystem.h>

namespace Nova {

namespace {

bool KeyLooksSafe(std::string_view key) {
    if (key.empty() || key.find("..") != std::string_view::npos) {
        return false;
    }
    if (key.front() == '/' || key.front() == '\\') {
        return false;
    }
    return true;
}

} // namespace

FileStorageProvider::FileStorageProvider(std::filesystem::path root) : m_Root(std::move(root)) {}

bool FileStorageProvider::ResolveKey(std::string_view key,
                                     std::filesystem::path& out,
                                     std::string& error) const {
    if (!KeyLooksSafe(key)) {
        error = "invalid storage key";
        return false;
    }
    out = m_Root / std::filesystem::path(std::string(key));
    return true;
}

StorageResult FileStorageProvider::Save(std::string_view key, std::string_view data) {
    StorageResult result;
    std::filesystem::path path;
    if (!ResolveKey(key, path, result.Error)) {
        return result;
    }
    const FileIOResult io = WriteTextFile(path, data);
    result.Ok = io.Ok;
    result.Error = io.Error;
    return result;
}

StorageResult FileStorageProvider::Load(std::string_view key) {
    StorageResult result;
    std::filesystem::path path;
    if (!ResolveKey(key, path, result.Error)) {
        return result;
    }
    const FileIOResult io = ReadTextFile(path);
    result.Ok = io.Ok;
    result.Error = io.Error;
    result.Data = io.Text;
    return result;
}

bool FileStorageProvider::Exists(std::string_view key) const {
    std::filesystem::path path;
    std::string error;
    if (!ResolveKey(key, path, error)) {
        return false;
    }
    return FileExists(path);
}

StorageResult FileStorageProvider::Remove(std::string_view key) {
    StorageResult result;
    std::filesystem::path path;
    if (!ResolveKey(key, path, result.Error)) {
        return result;
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) {
        result.Error = ec.message();
        return result;
    }
    result.Ok = true;
    return result;
}

FileStoragePlugin::FileStoragePlugin(std::shared_ptr<FileStorageProvider> provider)
    : m_Provider(std::move(provider)) {}

PluginInfo FileStoragePlugin::Info() const {
    PluginInfo info;
    info.Id = m_Provider ? m_Provider->Id() : "nova.storage.file";
    info.DisplayName = "File Storage";
    info.Version = "0.1.0";
    return info;
}

} // namespace Nova
