#pragma once

#include <Nova/Plugins/IAIProvider.h>
#include <Nova/Plugins/IPlugin.h>

#include <memory>

namespace Nova {

class NullAIProvider : public IAIProvider {
public:
    std::string Id() const override { return "nova.ai.null"; }
    bool IsAvailable() const override { return false; }
    ProviderTextResult GenerateText(const std::string& prompt) override;
};

class NullAIPlugin : public IPlugin {
public:
    explicit NullAIPlugin(std::shared_ptr<NullAIProvider> provider);

    PluginInfo Info() const override;
    void SetEnabled(bool enabled) override { m_Enabled = enabled; }
    bool IsEnabled() const override { return m_Enabled; }

private:
    std::shared_ptr<NullAIProvider> m_Provider;
    bool m_Enabled = true;
};

} // namespace Nova
