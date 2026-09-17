#include <Nova/Plugins/NullAIProvider.h>

namespace Nova {

ProviderTextResult NullAIProvider::GenerateText(const std::string& prompt) {
    (void)prompt;
    ProviderTextResult result;
    result.Error = "no AI provider configured";
    return result;
}

NullAIPlugin::NullAIPlugin(std::shared_ptr<NullAIProvider> provider)
    : m_Provider(std::move(provider)) {}

PluginInfo NullAIPlugin::Info() const {
    PluginInfo info;
    info.Id = m_Provider ? m_Provider->Id() : "nova.ai.null";
    info.DisplayName = "Null AI";
    info.Version = "0.1.0";
    return info;
}

} // namespace Nova
