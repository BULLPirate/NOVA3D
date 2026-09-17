#include <Nova/Plugins/PluginRegistry.h>

namespace Nova {

bool PluginRegistry::Register(std::unique_ptr<IPlugin> plugin) {
    if (!plugin) {
        return false;
    }
    const PluginInfo info = plugin->Info();
    if (info.Id.empty() || Find(info.Id) != nullptr) {
        return false;
    }
    if (!plugin->OnLoad()) {
        return false;
    }
    m_Plugins.push_back(std::move(plugin));
    return true;
}

IPlugin* PluginRegistry::Find(const std::string& id) const {
    for (const auto& plugin : m_Plugins) {
        if (plugin->Info().Id == id) {
            return plugin.get();
        }
    }
    return nullptr;
}

bool PluginRegistry::SetEnabled(const std::string& id, bool enabled) {
    IPlugin* plugin = Find(id);
    if (!plugin) {
        return false;
    }
    plugin->SetEnabled(enabled);
    return true;
}

bool PluginRegistry::Unload(const std::string& id) {
    for (auto it = m_Plugins.begin(); it != m_Plugins.end(); ++it) {
        if ((*it)->Info().Id == id) {
            (*it)->OnUnload();
            m_Plugins.erase(it);
            return true;
        }
    }
    return false;
}

std::vector<PluginInfo> PluginRegistry::List() const {
    std::vector<PluginInfo> infos;
    infos.reserve(m_Plugins.size());
    for (const auto& plugin : m_Plugins) {
        infos.push_back(plugin->Info());
    }
    return infos;
}

} // namespace Nova
