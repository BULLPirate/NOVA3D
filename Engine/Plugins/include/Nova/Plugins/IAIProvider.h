#pragma once

#include <string>

namespace Nova {

struct ProviderTextResult {
    bool Ok = false;
    std::string Text;
    std::string Error;
};

/// Engine talks to this interface only. Concrete vendors live in plugins.
class IAIProvider {
public:
    virtual ~IAIProvider() = default;
    virtual std::string Id() const = 0;
    virtual bool IsAvailable() const = 0;
    virtual ProviderTextResult GenerateText(const std::string& prompt) = 0;
};

} // namespace Nova
