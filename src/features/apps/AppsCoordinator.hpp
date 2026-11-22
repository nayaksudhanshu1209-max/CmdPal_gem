#pragma once

#include "core/CoreServices.hpp"
#include "features/apps/AppsContract.hpp"

#include <mutex>
#include <string>
#include <vector>

namespace cmdpal::features::apps {

class AppsCoordinator : public core::IFeature,
                        public core::ISearchProvider,
                        public IAppsService {
public:
    explicit AppsCoordinator(std::vector<std::string> customScanDirs = {});

    // IFeature
    std::string id() const override { return "apps"; }
    std::string displayName() const override { return "Application Search"; }
    void initialize(core::CoreServices& core) override;
    void shutdown() override;
    core::ISearchProvider* searchProvider() override { return this; }
    std::vector<core::SettingDescriptor> settingDescriptors() const override;

    // ISearchProvider
    std::string providerId() const override { return "apps"; }
    core::SearchCategory category() const override { return core::SearchCategory::Apps; }
    std::vector<core::SearchResult> query(const core::QueryContext& ctx) override;
    core::ActionResult execute(const core::SearchResult& item) override;

    // IAppsService
    void refreshApps() override;
    std::vector<AppEntry> cachedApps() const override;
    void hideApp(const std::string& appId) override;
    void restoreApp(const std::string& appId) override;

    // Exposed for deterministic testing
    static std::vector<AppEntry> scanDirectories(const std::vector<std::string>& dirs);

private:
    core::CoreServices* core_ = nullptr;
    std::vector<std::string> customScanDirs_;
    mutable std::mutex mu_;
    std::vector<AppEntry> cache_;
    bool initializedCache_ = false;
};

}  // namespace cmdpal::features::apps
