#pragma once

#include "bootstrap/FeatureRegistry.hpp"
#include "core/CoreServices.hpp"
#include "core/EventBus.hpp"
#include "core/IPlatformServices.hpp"
#include "core/SettingsStore.hpp"
#include "core/UsageTracker.hpp"

#include <memory>
#include <string>

namespace cmdpal::bootstrap {

class ApplicationContext {
public:
    ApplicationContext(std::unique_ptr<core::IPlatformServices> platform,
                       std::string dataDir,
                       std::vector<std::string> customAppScanDirs = {});
    ~ApplicationContext();

    core::CoreServices& core() { return *core_; }
    FeatureRegistry& registry() { return registry_; }

    void startDeferredBackgroundWork();
    void shutdown();

private:
    std::unique_ptr<core::IPlatformServices> platform_;
    std::unique_ptr<core::SettingsStore> settings_;
    std::unique_ptr<core::UsageTracker> usage_;
    core::EventBus events_;
    std::unique_ptr<core::CoreServices> core_;
    FeatureRegistry registry_;
    bool shutDown_ = false;
};

int runApplication(int argc, char** argv);

}  // namespace cmdpal::bootstrap
