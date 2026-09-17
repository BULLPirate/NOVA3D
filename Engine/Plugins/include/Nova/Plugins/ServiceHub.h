#pragma once

#include <Nova/Plugins/IAIProvider.h>
#include <Nova/Plugins/IStorageProvider.h>

#include <memory>

namespace Nova {

struct ServiceHub {
    std::shared_ptr<IAIProvider> AI;
    std::shared_ptr<IStorageProvider> Storage;
};

} // namespace Nova
