#include "bootstrap/Bootstrap.hpp"

#include "features/filesearch/FileSearchCoordinator.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace cmdpal::bootstrap {

ApplicationContext::ApplicationContext(std::unique_ptr<core::IPlatformServices> platform,
                                       std::string dataDir,
                                       std::vector<std::string> customAppScanDirs)
    : platform_(std::move(platform)) {
    auto known = platform_->getKnownFolders();
    std::string settingsPath = (fs::path(dataDir) / "settings.json").string();
    std::string usagePath = (fs::path(dataDir) / "usage.json").string();

    settings_ = std::make_unique<core::SettingsStore>(settingsPath, known);
    settings_->load();

    usage_ = std::make_unique<core::UsageTracker>(usagePath);
    usage_->load();

    core_ = std::make_unique<core::CoreServices>(*platform_, *settings_, *usage_, events_, dataDir);
    registry_.registerAllFeatures(
        *core_,
        [this]() { shutdown(); },
        std::move(customAppScanDirs));
}

ApplicationContext::~ApplicationContext() {
    shutdown();
}

void ApplicationContext::startDeferredBackgroundWork() {
    if (auto* fs = registry_.fileSearch()) {
        fs->startBackgroundIndexing();
    }
}

void ApplicationContext::shutdown() {
    if (shutDown_) return;
    shutDown_ = true;
    registry_.shutdownAll();
    if (settings_) settings_->save();
    if (usage_) usage_->save();
}

}  // namespace cmdpal::bootstrap
