#pragma once

#include <string>
#include <vector>

namespace Nova {

struct PluginInfo {
    std::string Id;
    std::string DisplayName;
    std::string Version = "0.1.0";
    std::vector<std::string> Dependencies;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;

    virtual PluginInfo Info() const = 0;
    virtual bool OnLoad() { return true; }
    virtual void OnUnload() {}
    virtual void SetEnabled(bool enabled) = 0;
    virtual bool IsEnabled() const = 0;
};

} // namespace Nova
