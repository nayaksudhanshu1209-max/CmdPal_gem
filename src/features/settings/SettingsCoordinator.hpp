#pragma once

#include "core/CoreServices.hpp"
#include "features/settings/SettingsContract.hpp"

#include <functional>
#include <string>
#include <vector>

namespace cmdpal::features::settings {

class SettingsCoordinator : public core::IFeature,
                            public core::ISearchProvider,
                            public ISettingsUiService {
public:
    using OpenWindowCallback = std::function<void(const std::string& section)>;

    explicit SettingsCoordinator(OpenWindowCallback onOpenWindow = nullptr);

    void setOpenWindowCallback(OpenWindowCallback cb) { onOpenWindow_ = std::move(cb); }

    // IFeature
    std::string id() const override { return "settings"; }
    std::string displayName() const override { return "Settings"; }
    void initialize(core::CoreServices& core) override;
    void shutdown() override;
    core::ISearchProvider* searchProvider() override { return this; }
    std::vector<core::SettingDescriptor> settingDescriptors() const override;

    // ISearchProvider
    std::string providerId() const override { return "settings"; }
    core::SearchCategory category() const override { return core::SearchCategory::Settings; }
    std::vector<core::SearchResult> query(const core::QueryContext& ctx) override;
    core::ActionResult execute(const core::SearchResult& item) override;

    // ISettingsUiService
    void openSettings(const std::string& section = "general") override;

    // Applies and persists updated settings (including OS startup registration)
    bool applySettings(const core::AppSettings& next);

private:
    core::CoreServices* core_ = nullptr;
    OpenWindowCallback onOpenWindow_;
};

}  // namespace cmdpal::features::settings
