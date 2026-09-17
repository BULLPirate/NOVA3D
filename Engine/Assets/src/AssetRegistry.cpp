#include <Nova/Assets/AssetRegistry.h>

namespace Nova {

AssetId AssetRegistry::RegisterPath(std::string projectRelativePath) {
    if (projectRelativePath.empty()) {
        return AssetId::Nil();
    }
    if (const auto existing = m_IdByPath.find(projectRelativePath); existing != m_IdByPath.end()) {
        return existing->second;
    }
    const AssetId id = AssetId::Generate();
    m_IdByPath.emplace(projectRelativePath, id);
    m_PathById.emplace(id, std::move(projectRelativePath));
    return id;
}

std::optional<std::string> AssetRegistry::PathFor(AssetId id) const {
    const auto it = m_PathById.find(id);
    if (it == m_PathById.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<AssetId> AssetRegistry::IdFor(const std::string& projectRelativePath) const {
    const auto it = m_IdByPath.find(projectRelativePath);
    if (it == m_IdByPath.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<AssetId> AssetRegistry::AllIds() const {
    std::vector<AssetId> ids;
    ids.reserve(m_PathById.size());
    for (const auto& [id, path] : m_PathById) {
        (void)path;
        ids.push_back(id);
    }
    return ids;
}

} // namespace Nova
