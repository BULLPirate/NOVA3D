#pragma once

#include <Nova/Assets/AssetId.h>

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Nova {

/// Maps stable AssetIds to project-relative paths so moving a file does not
/// have to break every scene if the registry is updated.
class AssetRegistry {
public:
    AssetId RegisterPath(std::string projectRelativePath);
    std::optional<std::string> PathFor(AssetId id) const;
    std::optional<AssetId> IdFor(const std::string& projectRelativePath) const;
    std::vector<AssetId> AllIds() const;
    std::size_t Count() const { return m_PathById.size(); }

private:
    std::unordered_map<AssetId, std::string, GuidHash> m_PathById;
    std::unordered_map<std::string, AssetId> m_IdByPath;
};

} // namespace Nova
