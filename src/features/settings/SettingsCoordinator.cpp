#include "features/settings/SettingsCoordinator.hpp"

#include "core/SearchMatch.hpp"

#include <algorithm>

namespace cmdpal::features::settings {

SettingsCoordinator::SettingsCoordinator(OpenWindowCallback onOpenWindow)
    : onOpenWindow_(std::move(onOpenWindow)) {}

void SettingsCoordinator::initialize(core::CoreServices& core) {
    core_ = &core;
    core_->events().subscribe<core::OpenSettingsEvent>(
        [this](const core::OpenSettingsEvent& ev) { openSettings(ev.section); });
}

void SettingsCoordinator::shutdown() {
    core_ = nullptr;
}

std::vector<core::SettingDescriptor> SettingsCoordinator::settingDescriptors() const {
    return {
        {"settings.startup", "Run at Windows Startup", "Automatically launch CmdPal on sign-in via HKCU Run",
         "general", {"startup", "boot", "autostart", "windows", "login"}},
        {"settings.reset", "Reset All Settings", "Restore all CmdPal settings to factory defaults",
         "general", {"reset", "defaults"}}
    };
}

std::vector<core::SearchResult> SettingsCoordinator::query(const core::QueryContext& ctx) {
    if (!core_ || ctx.activeChip.has_value()) return {};
    if (ctx.trimmedQuery.empty()) return {};

    std::vector<core::SearchResult> results;
    for (const auto& desc : core_->settingDescriptors()) {
        double best = core::scoreMatch(ctx.trimmedQuery, desc.title, desc.description);
        for (const auto& kw : desc.keywords) {
            best = std::max(best, core::scoreMatch(ctx.trimmedQuery, kw, "") * 0.9);
        }
        if (best <= 0.0) continue;

        core::SearchResult r;
        r.id = "setting:" + desc.key;
        r.title = desc.title;
        r.subtitle = "Settings > " + desc.section + " — " + desc.description;
        r.category = core::SearchCategory::Settings;
        r.categoryLabel = "Setting";
        r.score = best * 0.92;
        r.providerId = providerId();
        r.actionId = desc.section;
        results.push_back(std::move(r));
    }
    return results;
}

core::ActionResult SettingsCoordinator::execute(const core::SearchResult& item) {
    std::string section = item.actionId.empty() ? "general" : item.actionId;
    openSettings(section);
    return {true, true, "Opened settings section: " + section, section, ""};
}

void SettingsCoordinator::openSettings(const std::string& section) {
    if (onOpenWindow_) {
        onOpenWindow_(section);
    }
}

bool SettingsCoordinator::applySettings(const core::AppSettings& next) {
    if (!core_) return false;
    auto current = core_->settings().snapshot();
    if (current.runAtStartup != next.runAtStartup) {
        core_->platform().setStartupEnabled(next.runAtStartup);
    }
    core_->settings().update(next);
    core_->events().publish(core::SettingsChangedEvent{});
    return true;
}

}  // namespace cmdpal::features::settings
