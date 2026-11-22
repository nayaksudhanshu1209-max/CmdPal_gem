#include "features/quickfolders/QuickFoldersCoordinator.hpp"

#include "core/SearchMatch.hpp"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace cmdpal::features::quickfolders {

void QuickFoldersCoordinator::initialize(core::CoreServices& core) {
    core_ = &core;
}

void QuickFoldersCoordinator::shutdown() {
    core_ = nullptr;
}

std::vector<core::SettingDescriptor> QuickFoldersCoordinator::settingDescriptors() const {
    return {
        {"quickfolders.manage", "Quick Folders", "Add, edit, reorder, enable/disable, or remove Quick Folders",
         "files", {"quick", "folder", "folders", "shortcut"}}
    };
}

bool QuickFoldersCoordinator::addFolder(const std::string& label, const std::string& path, bool indexAsRoot) {
    if (!core_) return false;
    std::string cleanLabel = core::trimAscii(label);
    std::string cleanPath = core::trimAscii(path);
    if (cleanLabel.empty() || cleanPath.empty()) return false;

    auto s = core_->settings().snapshot();
    core::QuickFolderItem item;
    item.id = "qf-" + std::to_string(core_->platform().currentTimeMs()) + "-" + std::to_string(s.quickFolders.size());
    item.label = cleanLabel;
    item.path = cleanPath;
    item.enabled = true;
    item.indexAsRoot = indexAsRoot;
    s.quickFolders.push_back(std::move(item));
    core_->settings().update(s);
    core_->events().publish(core::SettingsChangedEvent{});
    return true;
}

bool QuickFoldersCoordinator::editFolder(const std::string& id,
                                         const std::string& label,
                                         const std::string& path,
                                         bool indexAsRoot) {
    if (!core_) return false;
    std::string cleanLabel = core::trimAscii(label);
    std::string cleanPath = core::trimAscii(path);
    if (cleanLabel.empty() || cleanPath.empty()) return false;

    auto s = core_->settings().snapshot();
    for (auto& q : s.quickFolders) {
        if (q.id == id) {
            q.label = cleanLabel;
            q.path = cleanPath;
            q.indexAsRoot = indexAsRoot;
            core_->settings().update(s);
            core_->events().publish(core::SettingsChangedEvent{});
            return true;
        }
    }
    return false;
}

bool QuickFoldersCoordinator::removeFolder(const std::string& id) {
    if (!core_) return false;
    auto s = core_->settings().snapshot();
    auto oldSize = s.quickFolders.size();
    // Removing a Quick Folder intentionally does NOT remove any FileRootItem
    s.quickFolders.erase(
        std::remove_if(s.quickFolders.begin(), s.quickFolders.end(),
                       [&](const core::QuickFolderItem& q) { return q.id == id; }),
        s.quickFolders.end());
    if (s.quickFolders.size() == oldSize) return false;
    core_->settings().update(s);
    core_->events().publish(core::SettingsChangedEvent{});
    return true;
}

bool QuickFoldersCoordinator::setEnabled(const std::string& id, bool enabled) {
    if (!core_) return false;
    auto s = core_->settings().snapshot();
    for (auto& q : s.quickFolders) {
        if (q.id == id) {
            q.enabled = enabled;
            core_->settings().update(s);
            core_->events().publish(core::SettingsChangedEvent{});
            return true;
        }
    }
    return false;
}

bool QuickFoldersCoordinator::moveFolder(const std::string& id, int delta) {
    if (!core_ || delta == 0) return false;
    auto s = core_->settings().snapshot();
    for (size_t i = 0; i < s.quickFolders.size(); ++i) {
        if (s.quickFolders[i].id == id) {
            int target = static_cast<int>(i) + delta;
            if (target < 0 || target >= static_cast<int>(s.quickFolders.size())) return false;
            std::swap(s.quickFolders[i], s.quickFolders[static_cast<size_t>(target)]);
            core_->settings().update(s);
            core_->events().publish(core::SettingsChangedEvent{});
            return true;
        }
    }
    return false;
}

std::vector<core::QuickFolderItem> QuickFoldersCoordinator::listFolders() const {
    if (!core_) return {};
    return core_->settings().snapshot().quickFolders;
}

std::vector<core::SearchResult> QuickFoldersCoordinator::query(const core::QueryContext& ctx) {
    if (!core_ || ctx.activeChip.has_value()) return {};

    auto s = core_->settings().snapshot();
    std::vector<core::SearchResult> results;
    for (const auto& qf : s.quickFolders) {
        if (!qf.enabled) continue;
        std::string resId = "qf:" + qf.id;
        if (core_->usage().isResultHidden(resId)) continue;

        double sc = ctx.trimmedQuery.empty()
                        ? 13.0
                        : core::scoreMatch(ctx.trimmedQuery, qf.label, qf.path);
        if (sc <= 0.0) continue;

        core::SearchResult r;
        r.id = resId;
        r.title = qf.label;
        r.subtitle = qf.path;
        r.category = core::SearchCategory::QuickFolders;
        r.categoryLabel = "Quick Folder";
        r.score = sc;
        r.providerId = providerId();
        r.actionId = qf.path;
        r.canHide = true;
        results.push_back(std::move(r));
    }
    return results;
}

core::ActionResult QuickFoldersCoordinator::execute(const core::SearchResult& item) {
    if (!core_ || item.actionId.empty()) {
        return {false, false, "Missing folder path", "", ""};
    }
    bool ok = core_->platform().openShellItem(item.actionId);
    if (ok) {
        core_->usage().recordExecution(item.id, core_->platform().currentTimeMs());
    }
    return {ok, ok, ok ? "Opened folder " + item.title : "Failed to open folder", "", ""};
}

}  // namespace cmdpal::features::quickfolders
