#include "features/apps/AppsCoordinator.hpp"

#include "core/SearchMatch.hpp"

#include <algorithm>
#include <filesystem>
#include <set>

namespace fs = std::filesystem;

namespace cmdpal::features::apps {

namespace {

bool isAppShortcutExt(const std::string& extLower) {
    return extLower == ".lnk" || extLower == ".exe" || extLower == ".appref-ms" || extLower == ".url";
}

bool isUninstallNoise(const std::string& nameLower) {
    return nameLower.find("uninstall") != std::string::npos ||
           nameLower.find("readme") != std::string::npos ||
           nameLower.find("help") != std::string::npos;
}

}  // namespace

AppsCoordinator::AppsCoordinator(std::vector<std::string> customScanDirs)
    : customScanDirs_(std::move(customScanDirs)) {}

void AppsCoordinator::initialize(core::CoreServices& core) {
    core_ = &core;
    core_->events().subscribe<core::AppsReloadRequestedEvent>(
        [this](const core::AppsReloadRequestedEvent&) { refreshApps(); });
}

void AppsCoordinator::shutdown() {
    core_ = nullptr;
}

std::vector<core::SettingDescriptor> AppsCoordinator::settingDescriptors() const {
    return {
        {"apps.hidden", "Hidden Applications", "Manage and restore applications hidden from search results",
         "categories", {"hidden", "apps", "restore", "hide"}}
    };
}

std::vector<AppEntry> AppsCoordinator::scanDirectories(const std::vector<std::string>& dirs) {
    std::vector<AppEntry> out;
    std::set<std::string> seenIds;

    for (const auto& dir : dirs) {
        if (dir.empty()) continue;
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;

        auto it = fs::recursive_directory_iterator(
            dir, fs::directory_options::skip_permission_denied, ec);
        for (; it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (it.depth() > 4) {
                it.disable_recursion_pending();
                continue;
            }
            // Do not follow symlinks to avoid loops
            if (it->is_symlink(ec)) {
                it.disable_recursion_pending();
                continue;
            }
            if (!it->is_regular_file(ec)) continue;

            std::string ext = core::toLowerAscii(it->path().extension().string());
            if (!isAppShortcutExt(ext)) continue;

            std::string stem = it->path().stem().string();
            if (stem.empty()) continue;
            std::string stemLower = core::toLowerAscii(stem);
            if (isUninstallNoise(stemLower)) continue;

            std::string id = "app:" + stemLower;
            if (seenIds.count(id)) continue;
            seenIds.insert(id);

            AppEntry entry;
            entry.id = std::move(id);
            entry.name = std::move(stem);
            entry.launchPath = it->path().string();
            entry.sourceType = "StartMenu";
            out.push_back(std::move(entry));
        }
    }

    std::sort(out.begin(), out.end(), [](const AppEntry& a, const AppEntry& b) {
        return core::toLowerAscii(a.name) < core::toLowerAscii(b.name);
    });
    return out;
}

void AppsCoordinator::refreshApps() {
    std::vector<std::string> dirs = customScanDirs_;
    if (dirs.empty() && core_) {
        auto kf = core_->platform().getKnownFolders();
        if (!kf.startMenuUser.empty()) dirs.push_back(kf.startMenuUser);
        if (!kf.startMenuCommon.empty()) dirs.push_back(kf.startMenuCommon);
    }
    auto scanned = scanDirectories(dirs);
    std::lock_guard<std::mutex> lock(mu_);
    cache_ = std::move(scanned);
    initializedCache_ = true;
}

std::vector<AppEntry> AppsCoordinator::cachedApps() const {
    std::lock_guard<std::mutex> lock(mu_);
    return cache_;
}

void AppsCoordinator::hideApp(const std::string& appId) {
    if (core_) core_->usage().hideApp(appId);
}

void AppsCoordinator::restoreApp(const std::string& appId) {
    if (core_) core_->usage().restoreApp(appId);
}

std::vector<core::SearchResult> AppsCoordinator::query(const core::QueryContext& ctx) {
    if (!core_) return {};
    if (ctx.activeChip.has_value()) return {};

    bool needScan = false;
    {
        std::lock_guard<std::mutex> lock(mu_);
        needScan = !initializedCache_;
    }
    if (needScan) refreshApps();

    std::vector<AppEntry> appsCopy = cachedApps();
    std::vector<core::SearchResult> results;

    for (const auto& app : appsCopy) {
        if (core_->usage().isAppHidden(app.id)) continue;
        if (core_->usage().isResultHidden(app.id)) continue;

        double s = ctx.trimmedQuery.empty()
                       ? 15.0
                       : core::scoreMatch(ctx.trimmedQuery, app.name, app.launchPath);
        if (s <= 0.0) continue;

        core::SearchResult r;
        r.id = app.id;
        r.title = app.name;
        r.subtitle = app.launchPath;
        r.category = core::SearchCategory::Apps;
        r.categoryLabel = "Application";
        r.iconPath = app.launchPath;
        r.score = s;
        r.providerId = providerId();
        r.actionId = app.launchPath;
        r.canHide = true;
        r.isApp = true;
        results.push_back(std::move(r));
    }
    return results;
}

core::ActionResult AppsCoordinator::execute(const core::SearchResult& item) {
    if (!core_ || item.actionId.empty()) {
        return {false, false, "Missing application path", "", ""};
    }
    bool ok = core_->platform().openShellItem(item.actionId);
    if (ok) {
        core_->usage().recordExecution(item.id, core_->platform().currentTimeMs());
    }
    return {ok, ok, ok ? "Launched " + item.title : "Failed to launch " + item.title, "", ""};
}

}  // namespace cmdpal::features::apps
