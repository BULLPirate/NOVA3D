#pragma once

#include <Nova/Plugins/IPlugin.h>

#include <memory>
#include <string>
#include <vector>

namespace Nova {

class PluginRegistry {
public:
    bool Register(std::unique_ptr<IPlugin> plugin);
    IPlugin* Find(const std::string& id) const;
    bool SetEnabled(const std::string& id, bool enabled);
    bool Unload(const std::string& id);
    std::vector<PluginInfo> List() const;
    std::size_t Count() const { return m_Plugins.size(); }

private:
    std::vector<std::unique_ptr<IPlugin>> m_Plugins;
};

} // namespace Nova
