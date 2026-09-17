#pragma once

#include <string>
#include <string_view>

namespace Nova {

struct StorageResult {
    bool Ok = false;
    std::string Error;
    std::string Data;
};

/// Key/value persistence. Games never include SQLite or a cloud SDK here.
class IStorageProvider {
public:
    virtual ~IStorageProvider() = default;
    virtual std::string Id() const = 0;
    virtual StorageResult Save(std::string_view key, std::string_view data) = 0;
    virtual StorageResult Load(std::string_view key) = 0;
    virtual bool Exists(std::string_view key) const = 0;
    virtual StorageResult Remove(std::string_view key) = 0;
};

} // namespace Nova
