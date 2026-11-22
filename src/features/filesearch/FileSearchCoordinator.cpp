#include "features/filesearch/FileSearchCoordinator.hpp"

#include "core/Json.hpp"
#include "core/SearchMatch.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace cmdpal::features::filesearch {

FileSearchCoordinator::~FileSearchCoordinator() {
    shutdown();
}

void FileSearchCoordinator::initialize(core::CoreServices& core) {
    core_ = &core;
    stopRequested_ = false;
    loadPersistedIndex();

    core_->events().subscribe<core::IndexStatusChangedEvent>(
        [this](const core::IndexStatusChangedEvent& ev) {
            if (ev.status == "rebuild_requested") {
                rebuildIndex();
            }
        });
}

void FileSearchCoordinator::shutdown() {
    stopRequested_ = true;
    paused_ = false;
    workerCv_.notify_all();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    core_ = nullptr;
}

std::vector<core::SettingDescriptor> FileSearchCoordinator::settingDescriptors() const {
    return {
        {"filesearch.roots", "File Search Roots", "Add, remove, or toggle Desktop, Documents, Downloads, and custom folders",
         "files", {"file", "index", "desktop", "documents", "downloads", "root"}},
        {"filesearch.status", "File Index Status & Rebuild", "Pause, resume, or rebuild the local file search index",
         "files", {"rebuild", "pause", "resume", "indexing"}}
    };
}

std::string FileSearchCoordinator::indexFilePath() const {
    if (!core_) return "file_index.json";
    return (fs::path(core_->dataDir()) / "file_index.json").string();
}

bool FileSearchCoordinator::isPathInsideRoot(const std::string& rootPath, const std::string& candidatePath) {
    std::error_code ec;
    fs::path r = fs::weakly_canonical(fs::path(rootPath), ec);
    if (ec) r = fs::path(rootPath).lexically_normal();
    fs::path c = fs::weakly_canonical(fs::path(candidatePath), ec);
    if (ec) c = fs::path(candidatePath).lexically_normal();

    auto rIt = r.begin();
    auto cIt = c.begin();
    for (; rIt != r.end(); ++rIt, ++cIt) {
        if (cIt == c.end()) return false;
        if (core::toLowerAscii(rIt->string()) != core::toLowerAscii(cIt->string())) return false;
    }
    return true;
}

void FileSearchCoordinator::loadPersistedIndex() {
    std::ifstream in(indexFilePath(), std::ios::binary);
    if (!in) return;
    std::ostringstream oss;
    oss << in.rdbuf();
    auto parsed = core::JsonValue::parse(oss.str());
    if (!parsed || !parsed->isArray()) return;

    std::vector<IndexedFileRecord> loaded;
    for (const auto& item : parsed->asArray()) {
        if (!item.isObject()) continue;
        IndexedFileRecord rec;
        if (auto* v = item.get("rootId")) rec.rootId = v->asString();
        if (auto* v = item.get("name")) rec.name = v->asString();
        if (auto* v = item.get("fullPath")) rec.fullPath = v->asString();
        if (auto* v = item.get("extension")) rec.extension = v->asString();
        if (auto* v = item.get("sizeBytes")) rec.sizeBytes = static_cast<uint64_t>(v->asInt64(0));
        if (auto* v = item.get("modifiedSec")) rec.modifiedSec = v->asInt64(0);
        if (!rec.fullPath.empty()) loaded.push_back(std::move(rec));
    }

    std::lock_guard<std::mutex> lock(recordsMu_);
    records_ = std::move(loaded);
    persistedLoaded_ = true;
}

void FileSearchCoordinator::savePersistedIndex(const std::vector<IndexedFileRecord>& records) const {
    core::JsonArray arr;
    arr.reserve(records.size());
    for (const auto& r : records) {
        core::JsonObject o;
        o["rootId"] = core::JsonValue(r.rootId);
        o["name"] = core::JsonValue(r.name);
        o["fullPath"] = core::JsonValue(r.fullPath);
        o["extension"] = core::JsonValue(r.extension);
        o["sizeBytes"] = core::JsonValue(static_cast<int64_t>(r.sizeBytes));
        o["modifiedSec"] = core::JsonValue(r.modifiedSec);
        arr.push_back(core::JsonValue(std::move(o)));
    }
    std::error_code ec;
    fs::path p(indexFilePath());
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream out(indexFilePath(), std::ios::binary | std::ios::trunc);
    if (out) {
        out << core::JsonValue(std::move(arr)).dump(2) << "\n";
    }
}

std::vector<IndexedFileRecord> FileSearchCoordinator::scanConfiguredRoots(std::atomic<bool>* cancelFlag) {
    if (!core_) return {};
    auto settings = core_->settings().snapshot();

    std::set<std::string> excluded;
    for (const auto& ex : settings.excludedFolderNames) {
        excluded.insert(core::toLowerAscii(ex));
    }

    struct ActiveRoot {
        std::string id;
        std::string path;
        int maxDepth;
    };
    std::vector<ActiveRoot> roots;
    for (const auto& r : settings.fileRoots) {
        if (r.enabled && !r.path.empty()) {
            roots.push_back({r.id, r.path, r.maxDepth});
        }
    }
    for (const auto& qf : settings.quickFolders) {
        if (qf.enabled && qf.indexAsRoot && !qf.path.empty()) {
            roots.push_back({"qf:" + qf.id, qf.path, 8});
        }
    }

    std::vector<IndexedFileRecord> scanned;
    size_t throttleCounter = 0;

    for (const auto& root : roots) {
        if (cancelFlag && cancelFlag->load()) break;
        std::error_code ec;
        if (!fs::exists(root.path, ec) || !fs::is_directory(root.path, ec)) continue;
        // Never follow a symlinked root blindly
        if (fs::is_symlink(root.path, ec)) continue;

        auto it = fs::recursive_directory_iterator(
            root.path, fs::directory_options::skip_permission_denied, ec);
        for (; it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (cancelFlag && cancelFlag->load()) break;

            while (paused_.load() && (!cancelFlag || !cancelFlag->load())) {
                std::unique_lock<std::mutex> lk(workerMu_);
                workerCv_.wait_for(lk, std::chrono::milliseconds(100));
            }

            // Do not follow directory junctions / symlinks
            if (it->is_symlink(ec)) {
                it.disable_recursion_pending();
                continue;
            }

            std::string filename = it->path().filename().string();
            if (it->is_directory(ec)) {
                if (it.depth() >= root.maxDepth || excluded.count(core::toLowerAscii(filename))) {
                    it.disable_recursion_pending();
                }
                continue;
            }

            if (!it->is_regular_file(ec)) continue;
            std::string fullPath = it->path().string();
            if (!isPathInsideRoot(root.path, fullPath)) continue;

            IndexedFileRecord rec;
            rec.rootId = root.id;
            rec.name = std::move(filename);
            rec.fullPath = std::move(fullPath);
            rec.extension = core::toLowerAscii(it->path().extension().string());
            rec.sizeBytes = it->file_size(ec);
            if (ec) {
                rec.sizeBytes = 0;
                ec.clear();
            }
            auto ftime = it->last_write_time(ec);
            if (!ec) {
                rec.modifiedSec = static_cast<int64_t>(ftime.time_since_epoch().count());
            }
            scanned.push_back(std::move(rec));

            // Gentle throttling for low-end HDD/CPU hardware
            if (++throttleCounter % 200 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    return scanned;
}

void FileSearchCoordinator::indexSynchronouslyNow() {
    auto scanned = scanConfiguredRoots(&stopRequested_);
    {
        std::lock_guard<std::mutex> lock(recordsMu_);
        records_ = scanned;
    }
    savePersistedIndex(scanned);
    if (core_) {
        core_->events().publish(core::IndexStatusChangedEvent{"Indexed", scanned.size(), paused_.load()});
    }
}

void FileSearchCoordinator::workerLoop() {
    while (!stopRequested_.load()) {
        running_ = true;
        {
            std::lock_guard<std::mutex> lk(workerMu_);
            stateText_ = paused_.load() ? "Paused" : "Indexing";
        }

        auto scanned = scanConfiguredRoots(&stopRequested_);
        if (!stopRequested_.load()) {
            {
                std::lock_guard<std::mutex> lock(recordsMu_);
                records_ = scanned;
            }
            savePersistedIndex(scanned);
            {
                std::lock_guard<std::mutex> lk(workerMu_);
                stateText_ = paused_.load() ? "Paused" : "Ready";
            }
            if (core_) {
                core_->events().publish(core::IndexStatusChangedEvent{stateText_, scanned.size(), paused_.load()});
            }
        }
        running_ = false;

        std::unique_lock<std::mutex> lk(workerMu_);
        workerCv_.wait(lk, [this]() { return stopRequested_.load() || rebuildRequested_.load(); });
        rebuildRequested_ = false;
    }
}

void FileSearchCoordinator::startBackgroundIndexing() {
    if (workerThread_.joinable()) return;
    stopRequested_ = false;
    workerThread_ = std::thread([this]() { workerLoop(); });
}

void FileSearchCoordinator::rebuildIndex() {
    if (!workerThread_.joinable()) {
        startBackgroundIndexing();
    } else {
        rebuildRequested_ = true;
        workerCv_.notify_all();
    }
}

void FileSearchCoordinator::pauseIndexing() {
    paused_ = true;
    std::lock_guard<std::mutex> lk(workerMu_);
    stateText_ = "Paused";
}

void FileSearchCoordinator::resumeIndexing() {
    paused_ = false;
    {
        std::lock_guard<std::mutex> lk(workerMu_);
        stateText_ = running_.load() ? "Indexing" : "Ready";
    }
    workerCv_.notify_all();
}

IndexingStatus FileSearchCoordinator::status() const {
    IndexingStatus st;
    st.running = running_.load();
    st.paused = paused_.load();
    {
        std::lock_guard<std::mutex> lock(recordsMu_);
        st.indexedCount = records_.size();
    }
    std::lock_guard<std::mutex> lk(workerMu_);
    st.stateText = stateText_;
    return st;
}

bool FileSearchCoordinator::addRoot(const std::string& label, const std::string& path, int maxDepth) {
    if (!core_) return false;
    std::string cleanLabel = core::trimAscii(label);
    std::string cleanPath = core::trimAscii(path);
    if (cleanLabel.empty() || cleanPath.empty()) return false;

    auto s = core_->settings().snapshot();
    core::FileRootItem item;
    item.id = "root-" + std::to_string(core_->platform().currentTimeMs()) + "-" + std::to_string(s.fileRoots.size());
    item.label = cleanLabel;
    item.path = cleanPath;
    item.isDefault = false;
    item.enabled = true;
    item.maxDepth = std::clamp(maxDepth, 1, 32);
    s.fileRoots.push_back(std::move(item));
    core_->settings().update(s);
    core_->events().publish(core::SettingsChangedEvent{});
    return true;
}

bool FileSearchCoordinator::removeRoot(const std::string& rootId) {
    if (!core_) return false;
    auto s = core_->settings().snapshot();
    auto oldSize = s.fileRoots.size();
    s.fileRoots.erase(
        std::remove_if(s.fileRoots.begin(), s.fileRoots.end(),
                       [&](const core::FileRootItem& r) { return r.id == rootId; }),
        s.fileRoots.end());
    if (s.fileRoots.size() == oldSize) return false;
    core_->settings().update(s);
    {
        std::lock_guard<std::mutex> lock(recordsMu_);
        records_.erase(
            std::remove_if(records_.begin(), records_.end(),
                           [&](const IndexedFileRecord& rec) { return rec.rootId == rootId; }),
            records_.end());
    }
    core_->events().publish(core::SettingsChangedEvent{});
    return true;
}

bool FileSearchCoordinator::setRootEnabled(const std::string& rootId, bool enabled) {
    if (!core_) return false;
    auto s = core_->settings().snapshot();
    for (auto& r : s.fileRoots) {
        if (r.id == rootId) {
            r.enabled = enabled;
            core_->settings().update(s);
            if (!enabled) {
                std::lock_guard<std::mutex> lock(recordsMu_);
                records_.erase(
                    std::remove_if(records_.begin(), records_.end(),
                                   [&](const IndexedFileRecord& rec) { return rec.rootId == rootId; }),
                    records_.end());
            }
            core_->events().publish(core::SettingsChangedEvent{});
            return true;
        }
    }
    return false;
}

std::vector<core::SearchResult> FileSearchCoordinator::query(const core::QueryContext& ctx) {
    if (!core_ || ctx.activeChip.has_value()) return {};
    if (ctx.trimmedQuery.empty()) return {};  // Avoid flooding initial empty open with files

    auto settings = core_->settings().snapshot();
    std::set<std::string> enabledRootIds;
    for (const auto& r : settings.fileRoots) {
        if (r.enabled) enabledRootIds.insert(r.id);
    }
    for (const auto& qf : settings.quickFolders) {
        if (qf.enabled && qf.indexAsRoot) enabledRootIds.insert("qf:" + qf.id);
    }

    std::vector<IndexedFileRecord> copy;
    {
        std::lock_guard<std::mutex> lock(recordsMu_);
        copy = records_;
    }

    std::vector<core::SearchResult> results;
    for (const auto& rec : copy) {
        if (!enabledRootIds.count(rec.rootId)) continue;
        std::string resId = "file:" + rec.fullPath;
        if (core_->usage().isResultHidden(resId)) continue;

        double sc = core::scoreMatch(ctx.trimmedQuery, rec.name, rec.fullPath);
        if (sc <= 0.0) continue;

        core::SearchResult r;
        r.id = resId;
        r.title = rec.name;
        r.subtitle = rec.fullPath;
        r.category = core::SearchCategory::Files;
        r.categoryLabel = "File";
        r.iconPath = rec.fullPath;
        r.score = sc * 0.85;  // Slightly below exact apps/commands
        r.providerId = providerId();
        r.actionId = rec.fullPath;
        r.canHide = true;
        results.push_back(std::move(r));
        if (results.size() >= 40) break;
    }
    return results;
}

core::ActionResult FileSearchCoordinator::execute(const core::SearchResult& item) {
    if (!core_ || item.actionId.empty()) {
        return {false, false, "Missing file path", "", ""};
    }
    bool ok = core_->platform().openShellItem(item.actionId);
    if (ok) {
        core_->usage().recordExecution(item.id, core_->platform().currentTimeMs());
    }
    return {ok, ok, ok ? "Opened " + item.title : "Failed to open file", "", ""};
}

}  // namespace cmdpal::features::filesearch
