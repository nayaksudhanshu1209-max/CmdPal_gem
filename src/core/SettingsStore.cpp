#include "core/SettingsStore.hpp"

#include "core/Json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace cmdpal::core {

namespace {

std::string densityToStr(DensityMode d) {
    return d == DensityMode::Comfortable ? "comfortable" : "compact";
}
DensityMode strToDensity(const std::string& s) {
    return s == "comfortable" ? DensityMode::Comfortable : DensityMode::Compact;
}

std::string themeToStr(ThemeMode t) {
    if (t == ThemeMode::Light) return "light";
    if (t == ThemeMode::Dark) return "dark";
    return "system";
}
ThemeMode strToTheme(const std::string& s) {
    if (s == "light") return ThemeMode::Light;
    if (s == "dark") return ThemeMode::Dark;
    return ThemeMode::System;
}

std::string placementToStr(PalettePlacement p) {
    if (p == PalettePlacement::TopCenter) return "top_center";
    if (p == PalettePlacement::NearCursor) return "near_cursor";
    return "center";
}
PalettePlacement strToPlacement(const std::string& s) {
    if (s == "top_center") return PalettePlacement::TopCenter;
    if (s == "near_cursor") return PalettePlacement::NearCursor;
    return PalettePlacement::Center;
}

bool isSafeTemplateUrl(const std::string& url) {
    if (url.size() < 8) return false;
    std::string lower = url.substr(0, 8);
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower.rfind("https://", 0) == 0 || lower.rfind("http://", 0) == 0;
}

std::string joinPath(const std::string& base, const std::string& child) {
    if (base.empty()) return child;
    return (fs::path(base) / child).string();
}

}  // namespace

SettingsStore::SettingsStore(std::string filePath, const KnownFolders& defaults)
    : filePath_(std::move(filePath)), defaults_(defaults) {
    settings_ = makeDefaults(defaults_);
}

AppSettings SettingsStore::makeDefaults(const KnownFolders& folders) {
    AppSettings s;
    s.globalShortcut = ShortcutConfig{true, true, false, false, "Space"};
    s.runAtStartup = false;
    s.density = DensityMode::Compact;
    s.theme = ThemeMode::System;
    s.placement = PalettePlacement::Center;
    s.animationsEnabled = true;
    s.animationDurationMs = 90;
    s.showSuggestionsOnOpen = true;
    s.showSuggestionsWhileTyping = true;
    s.maxResults = 12;

    s.defaultWebSearchId = "google";
    s.webBangs = {
        {"google", "g", "Google Search", "https://www.google.com/search?q={query}", true},
        {"web", "w", "Web Search", "https://www.google.com/search?q={query}", true},
        {"youtube", "yt", "YouTube", "https://www.youtube.com/results?search_query={query}", true}
    };
    s.bookmarks = {
        {"bm-google", "Google", "https://www.google.com", "google"}
    };

    if (!folders.desktop.empty()) {
        s.fileRoots.push_back({"default-desktop", "Desktop", folders.desktop, true, true, 6});
    }
    if (!folders.documents.empty()) {
        s.fileRoots.push_back({"default-documents", "Documents", folders.documents, true, true, 8});
    }
    if (!folders.downloads.empty()) {
        s.fileRoots.push_back({"default-downloads", "Downloads", folders.downloads, true, true, 6});
    }

    if (!folders.documents.empty()) {
        s.notesDirectory = joinPath(folders.documents, "CmdPal Notes");
    } else if (!folders.appData.empty()) {
        s.notesDirectory = joinPath(folders.appData, "CmdPal/Notes");
    } else {
        s.notesDirectory = "CmdPalNotes";
    }
    return s;
}

AppSettings SettingsStore::validateAndSanitize(AppSettings s) {
    if (!s.globalShortcut.ctrl && !s.globalShortcut.alt &&
        !s.globalShortcut.shift && !s.globalShortcut.win) {
        s.globalShortcut.ctrl = true;
        s.globalShortcut.alt = true;
    }
    if (s.globalShortcut.key.empty()) {
        s.globalShortcut.key = "Space";
    }
    s.animationDurationMs = std::clamp(s.animationDurationMs, 0, 400);
    s.maxResults = std::clamp(s.maxResults, 4, 50);
    s.noteAutosaveDebounceMs = std::clamp(s.noteAutosaveDebounceMs, 150, 10000);

    // Filter out unsafe web bang templates
    s.webBangs.erase(
        std::remove_if(s.webBangs.begin(), s.webBangs.end(),
                       [](const WebBangRule& r) {
                           return r.alias.empty() || !isSafeTemplateUrl(r.urlTemplate);
                       }),
        s.webBangs.end());

    // Filter out unsafe bookmarks
    s.bookmarks.erase(
        std::remove_if(s.bookmarks.begin(), s.bookmarks.end(),
                       [](const BookmarkItem& b) {
                           return b.title.empty() || !isSafeTemplateUrl(b.url);
                       }),
        s.bookmarks.end());

    // Sanitize file roots
    for (auto& root : s.fileRoots) {
        root.maxDepth = std::clamp(root.maxDepth, 1, 32);
    }

    return s;
}

bool SettingsStore::load() {
    std::lock_guard<std::mutex> lock(mu_);
    recoveredFromMalformed_ = false;

    std::error_code ec;
    if (!fs::exists(filePath_, ec)) {
        settings_ = makeDefaults(defaults_);
        return true;
    }

    std::ifstream in(filePath_, std::ios::binary);
    if (!in) {
        settings_ = makeDefaults(defaults_);
        return false;
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    std::string raw = oss.str();

    auto parsed = JsonValue::parse(raw);
    if (!parsed || !parsed->isObject()) {
        // Backup malformed file and recover safely
        std::string backupPath = filePath_ + ".bak";
        fs::copy_file(filePath_, backupPath, fs::copy_options::overwrite_existing, ec);
        settings_ = makeDefaults(defaults_);
        recoveredFromMalformed_ = true;
        return false;
    }

    AppSettings s = makeDefaults(defaults_);
    const auto& root = *parsed;

    if (auto* v = root.get("runAtStartup")) s.runAtStartup = v->asBool(s.runAtStartup);
    if (auto* v = root.get("density")) s.density = strToDensity(v->asString());
    if (auto* v = root.get("theme")) s.theme = strToTheme(v->asString());
    if (auto* v = root.get("placement")) s.placement = strToPlacement(v->asString());
    if (auto* v = root.get("animationsEnabled")) s.animationsEnabled = v->asBool(s.animationsEnabled);
    if (auto* v = root.get("animationDurationMs")) s.animationDurationMs = v->asInt(s.animationDurationMs);
    if (auto* v = root.get("showSuggestionsOnOpen")) s.showSuggestionsOnOpen = v->asBool(s.showSuggestionsOnOpen);
    if (auto* v = root.get("showSuggestionsWhileTyping")) s.showSuggestionsWhileTyping = v->asBool(s.showSuggestionsWhileTyping);
    if (auto* v = root.get("maxResults")) s.maxResults = v->asInt(s.maxResults);
    if (auto* v = root.get("defaultWebSearchId")) s.defaultWebSearchId = v->asString(s.defaultWebSearchId);
    if (auto* v = root.get("indexFileContentsOptIn")) s.indexFileContentsOptIn = v->asBool(false);
    if (auto* v = root.get("notesDirectory")) {
        if (!v->asString().empty()) s.notesDirectory = v->asString();
    }
    if (auto* v = root.get("noteAutosaveDebounceMs")) s.noteAutosaveDebounceMs = v->asInt(s.noteAutosaveDebounceMs);

    if (auto* sc = root.get("globalShortcut"); sc && sc->isObject()) {
        if (auto* v = sc->get("ctrl")) s.globalShortcut.ctrl = v->asBool(true);
        if (auto* v = sc->get("alt")) s.globalShortcut.alt = v->asBool(true);
        if (auto* v = sc->get("shift")) s.globalShortcut.shift = v->asBool(false);
        if (auto* v = sc->get("win")) s.globalShortcut.win = v->asBool(false);
        if (auto* v = sc->get("key")) s.globalShortcut.key = v->asString("Space");
    }

    if (auto* cats = root.get("enabledCategories"); cats && cats->isObject()) {
        for (const auto& [k, v] : cats->asObject()) {
            s.enabledCategories[k] = v.asBool(true);
        }
    }

    if (auto* bangs = root.get("webBangs"); bangs && bangs->isArray()) {
        s.webBangs.clear();
        for (const auto& item : bangs->asArray()) {
            if (!item.isObject()) continue;
            WebBangRule r;
            if (auto* v = item.get("id")) r.id = v->asString();
            if (auto* v = item.get("alias")) r.alias = v->asString();
            if (auto* v = item.get("label")) r.label = v->asString();
            if (auto* v = item.get("urlTemplate")) r.urlTemplate = v->asString();
            if (auto* v = item.get("enabled")) r.enabled = v->asBool(true);
            s.webBangs.push_back(std::move(r));
        }
    }

    if (auto* bms = root.get("bookmarks"); bms && bms->isArray()) {
        s.bookmarks.clear();
        for (const auto& item : bms->asArray()) {
            if (!item.isObject()) continue;
            BookmarkItem b;
            if (auto* v = item.get("id")) b.id = v->asString();
            if (auto* v = item.get("title")) b.title = v->asString();
            if (auto* v = item.get("url")) b.url = v->asString();
            if (auto* v = item.get("bangId")) b.bangId = v->asString();
            s.bookmarks.push_back(std::move(b));
        }
    }

    if (auto* cmds = root.get("userCommands"); cmds && cmds->isArray()) {
        s.userCommands.clear();
        for (const auto& item : cmds->asArray()) {
            if (!item.isObject()) continue;
            UserCommandDef c;
            if (auto* v = item.get("id")) c.id = v->asString();
            if (auto* v = item.get("name")) c.name = v->asString();
            if (auto* v = item.get("description")) c.description = v->asString();
            if (auto* v = item.get("alias")) c.alias = v->asString();
            if (auto* v = item.get("category")) c.category = v->asString("Custom");
            if (auto* v = item.get("executable")) c.executable = v->asString();
            if (auto* v = item.get("workingDir")) c.workingDir = v->asString();
            if (auto* v = item.get("requiresConfirmation")) c.requiresConfirmation = v->asBool(false);
            if (auto* v = item.get("enabled")) c.enabled = v->asBool(true);
            if (auto* args = item.get("args"); args && args->isArray()) {
                for (const auto& a : args->asArray()) {
                    c.args.push_back(a.asString());
                }
            }
            s.userCommands.push_back(std::move(c));
        }
    }

    if (auto* qfs = root.get("quickFolders"); qfs && qfs->isArray()) {
        s.quickFolders.clear();
        for (const auto& item : qfs->asArray()) {
            if (!item.isObject()) continue;
            QuickFolderItem q;
            if (auto* v = item.get("id")) q.id = v->asString();
            if (auto* v = item.get("label")) q.label = v->asString();
            if (auto* v = item.get("path")) q.path = v->asString();
            if (auto* v = item.get("enabled")) q.enabled = v->asBool(true);
            if (auto* v = item.get("indexAsRoot")) q.indexAsRoot = v->asBool(false);
            s.quickFolders.push_back(std::move(q));
        }
    }

    if (auto* roots = root.get("fileRoots"); roots && roots->isArray()) {
        s.fileRoots.clear();
        for (const auto& item : roots->asArray()) {
            if (!item.isObject()) continue;
            FileRootItem f;
            if (auto* v = item.get("id")) f.id = v->asString();
            if (auto* v = item.get("label")) f.label = v->asString();
            if (auto* v = item.get("path")) f.path = v->asString();
            if (auto* v = item.get("isDefault")) f.isDefault = v->asBool(false);
            if (auto* v = item.get("enabled")) f.enabled = v->asBool(true);
            if (auto* v = item.get("maxDepth")) f.maxDepth = v->asInt(8);
            s.fileRoots.push_back(std::move(f));
        }
    }

    settings_ = validateAndSanitize(std::move(s));
    return true;
}

bool SettingsStore::save() const {
    AppSettings s;
    {
        std::lock_guard<std::mutex> lock(mu_);
        s = settings_;
    }

    JsonObject root;
    root["schemaVersion"] = JsonValue(s.schemaVersion);
    root["runAtStartup"] = JsonValue(s.runAtStartup);
    root["density"] = JsonValue(densityToStr(s.density));
    root["theme"] = JsonValue(themeToStr(s.theme));
    root["placement"] = JsonValue(placementToStr(s.placement));
    root["animationsEnabled"] = JsonValue(s.animationsEnabled);
    root["animationDurationMs"] = JsonValue(s.animationDurationMs);
    root["showSuggestionsOnOpen"] = JsonValue(s.showSuggestionsOnOpen);
    root["showSuggestionsWhileTyping"] = JsonValue(s.showSuggestionsWhileTyping);
    root["maxResults"] = JsonValue(s.maxResults);
    root["defaultWebSearchId"] = JsonValue(s.defaultWebSearchId);
    root["indexFileContentsOptIn"] = JsonValue(s.indexFileContentsOptIn);
    root["notesDirectory"] = JsonValue(s.notesDirectory);
    root["noteAutosaveDebounceMs"] = JsonValue(s.noteAutosaveDebounceMs);

    JsonObject sc;
    sc["ctrl"] = JsonValue(s.globalShortcut.ctrl);
    sc["alt"] = JsonValue(s.globalShortcut.alt);
    sc["shift"] = JsonValue(s.globalShortcut.shift);
    sc["win"] = JsonValue(s.globalShortcut.win);
    sc["key"] = JsonValue(s.globalShortcut.key);
    root["globalShortcut"] = JsonValue(std::move(sc));

    JsonObject cats;
    for (const auto& [k, v] : s.enabledCategories) cats[k] = JsonValue(v);
    root["enabledCategories"] = JsonValue(std::move(cats));

    JsonArray bangs;
    for (const auto& b : s.webBangs) {
        JsonObject o;
        o["id"] = JsonValue(b.id);
        o["alias"] = JsonValue(b.alias);
        o["label"] = JsonValue(b.label);
        o["urlTemplate"] = JsonValue(b.urlTemplate);
        o["enabled"] = JsonValue(b.enabled);
        bangs.push_back(JsonValue(std::move(o)));
    }
    root["webBangs"] = JsonValue(std::move(bangs));

    JsonArray bms;
    for (const auto& b : s.bookmarks) {
        JsonObject o;
        o["id"] = JsonValue(b.id);
        o["title"] = JsonValue(b.title);
        o["url"] = JsonValue(b.url);
        o["bangId"] = JsonValue(b.bangId);
        bms.push_back(JsonValue(std::move(o)));
    }
    root["bookmarks"] = JsonValue(std::move(bms));

    JsonArray cmds;
    for (const auto& c : s.userCommands) {
        JsonObject o;
        o["id"] = JsonValue(c.id);
        o["name"] = JsonValue(c.name);
        o["description"] = JsonValue(c.description);
        o["alias"] = JsonValue(c.alias);
        o["category"] = JsonValue(c.category);
        o["executable"] = JsonValue(c.executable);
        o["workingDir"] = JsonValue(c.workingDir);
        o["requiresConfirmation"] = JsonValue(c.requiresConfirmation);
        o["enabled"] = JsonValue(c.enabled);
        JsonArray args;
        for (const auto& a : c.args) args.push_back(JsonValue(a));
        o["args"] = JsonValue(std::move(args));
        cmds.push_back(JsonValue(std::move(o)));
    }
    root["userCommands"] = JsonValue(std::move(cmds));

    JsonArray qfs;
    for (const auto& q : s.quickFolders) {
        JsonObject o;
        o["id"] = JsonValue(q.id);
        o["label"] = JsonValue(q.label);
        o["path"] = JsonValue(q.path);
        o["enabled"] = JsonValue(q.enabled);
        o["indexAsRoot"] = JsonValue(q.indexAsRoot);
        qfs.push_back(JsonValue(std::move(o)));
    }
    root["quickFolders"] = JsonValue(std::move(qfs));

    JsonArray roots;
    for (const auto& f : s.fileRoots) {
        JsonObject o;
        o["id"] = JsonValue(f.id);
        o["label"] = JsonValue(f.label);
        o["path"] = JsonValue(f.path);
        o["isDefault"] = JsonValue(f.isDefault);
        o["enabled"] = JsonValue(f.enabled);
        o["maxDepth"] = JsonValue(f.maxDepth);
        roots.push_back(JsonValue(std::move(o)));
    }
    root["fileRoots"] = JsonValue(std::move(roots));

    std::error_code ec;
    fs::path p(filePath_);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path(), ec);
    }

    std::string tmpPath = filePath_ + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << JsonValue(std::move(root)).dump(2) << "\n";
        if (!out.good()) return false;
    }
    fs::rename(tmpPath, filePath_, ec);
    if (ec) {
        fs::remove(filePath_, ec);
        ec.clear();
        fs::rename(tmpPath, filePath_, ec);
    }
    return !ec;
}

AppSettings SettingsStore::snapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return settings_;
}

void SettingsStore::update(const AppSettings& next) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        settings_ = validateAndSanitize(next);
    }
    save();
}

void SettingsStore::resetToDefaults() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        settings_ = makeDefaults(defaults_);
    }
    save();
}

}  // namespace cmdpal::core
