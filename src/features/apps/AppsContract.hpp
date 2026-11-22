#pragma once

#include "core/Contracts.hpp"
#include "features/apps/AppsModels.hpp"

#include <vector>

namespace cmdpal::features::apps {

class IAppsService {
public:
    virtual ~IAppsService() = default;
    virtual void refreshApps() = 0;
    virtual std::vector<AppEntry> cachedApps() const = 0;
    virtual void hideApp(const std::string& appId) = 0;
    virtual void restoreApp(const std::string& appId) = 0;
};

}  // namespace cmdpal::features::apps
