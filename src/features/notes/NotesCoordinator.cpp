#include "features/notes/NotesCoordinator.hpp"

#include "core/Json.hpp"
#include "core/SearchMatch.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace cmdpal::features::notes {

void NotesCoordinator::initialize(core::CoreServices& core) {
    core_ = &core;
    loadPrefs();
}

void NotesCoordinator::shutdown() {
    savePrefs();
    core_ = nullptr;
}

std::vector<core::SettingDescriptor> NotesCoordinator::settingDescriptors() const {
    return {
        {"notes.directory", "Notes Storage Directory", "Folder where local Markdown (.md) notes are stored",
         "notes", {"notes", "markdown", "directory", "folder", "autosave"}},
        {"notes.autosave", "Note Autosave Delay", "Debounce delay in milliseconds before saving note edits",
         "notes", {"notes", "autosave", "save"}}
    };
}

std::string NotesCoordinator::notesDir() const {
    if (!core_) return "CmdPalNotes";
    return core_->settings().snapshot().notesDirectory;
}

std::string NotesCoordinator::prefsPath() const {
    if (!core_) return "notes_prefs.json";
    return (fs::path(core_->dataDir()) / "notes_prefs.json").string();
}

std::string NotesCoordinator::sanitizeFileName(const std::string& title) {
    std::string out;
    for (unsigned char c : title) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == ' ') {
            out.push_back(static_cast<char>(c));
        }
    }
    out = core::trimAscii(out);
    if (out.empty()) out = "Untitled Note";
    return out;
}

void NotesCoordinator::loadPrefs() {
    std::ifstream in(prefsPath(), std::ios::binary);
    if (!in) return;
    std::ostringstream oss;
    oss << in.rdbuf();
    auto parsed = core::JsonValue::parse(oss.str());
    if (!parsed || !parsed->isObject()) return;

    toolbarUsageByNote_.clear();
    for (const auto& [noteId, val] : parsed->asObject()) {
        if (!val.isObject()) continue;
        auto& counts = toolbarUsageByNote_[noteId];
        for (const auto& [actId, cnt] : val.asObject()) {
            counts[actId] = cnt.asInt(0);
        }
    }
}

void NotesCoordinator::savePrefs() const {
    core::JsonObject root;
    for (const auto& [noteId, counts] : toolbarUsageByNote_) {
        core::JsonObject m;
        for (const auto& [actId, cnt] : counts) {
            m[actId] = core::JsonValue(cnt);
        }
        root[noteId] = core::JsonValue(std::move(m));
    }
    std::error_code ec;
    fs::path p(prefsPath());
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream out(prefsPath(), std::ios::binary | std::ios::trunc);
    if (out) out << core::JsonValue(std::move(root)).dump(2) << "\n";
}

std::vector<NoteMetadata> NotesCoordinator::listNotes() const {
    std::vector<NoteMetadata> notes;
    std::error_code ec;
    std::string dir = notesDir();
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return notes;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file(ec)) continue;
        if (core::toLowerAscii(entry.path().extension().string()) != ".md") continue;

        NoteMetadata m;
        m.id = entry.path().stem().string();
        m.title = m.id;
        m.filePath = entry.path().string();
        auto ftime = entry.last_write_time(ec);
        if (!ec) m.modifiedSec = static_cast<int64_t>(ftime.time_since_epoch().count());
        notes.push_back(std::move(m));
    }

    std::sort(notes.begin(), notes.end(), [](const NoteMetadata& a, const NoteMetadata& b) {
        return a.modifiedSec > b.modifiedSec;
    });
    return notes;
}

std::optional<NoteMetadata> NotesCoordinator::createNote(const std::string& title) {
    std::error_code ec;
    std::string dir = notesDir();
    fs::create_directories(dir, ec);
    if (ec) return std::nullopt;

    std::string base = sanitizeFileName(title);
    std::string candidate = base;
    int suffix = 1;
    while (fs::exists(fs::path(dir) / (candidate + ".md"), ec)) {
        candidate = base + " (" + std::to_string(++suffix) + ")";
    }

    fs::path fullPath = fs::path(dir) / (candidate + ".md");
    std::ofstream out(fullPath, std::ios::binary | std::ios::trunc);
    if (!out) return std::nullopt;
    out << "# " << candidate << "\n\n";
    out.flush();
    if (!out.good()) return std::nullopt;

    NoteMetadata m;
    m.id = candidate;
    m.title = candidate;
    m.filePath = fullPath.string();
    return m;
}

std::string NotesCoordinator::loadNoteContent(const std::string& noteId) const {
    fs::path p = fs::path(notesDir()) / (noteId + ".md");
    std::ifstream in(p, std::ios::binary);
    if (!in) return "";
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

bool NotesCoordinator::saveNoteContent(const std::string& noteId, const std::string& content) {
    if (noteId.empty()) return false;
    std::error_code ec;
    std::string dir = notesDir();
    fs::create_directories(dir, ec);
    if (ec) return false;

    fs::path target = fs::path(dir) / (noteId + ".md");
    fs::path tmp = fs::path(dir) / (noteId + ".md.tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << content;
        out.flush();
        if (!out.good()) return false;
    }
    fs::rename(tmp, target, ec);
    if (ec) {
        fs::remove(target, ec);
        ec.clear();
        fs::rename(tmp, target, ec);
    }
    return !ec;
}

bool NotesCoordinator::renameNote(const std::string& noteId, const std::string& newTitle) {
    std::string clean = sanitizeFileName(newTitle);
    if (clean == noteId) return true;
    std::error_code ec;
    fs::path oldP = fs::path(notesDir()) / (noteId + ".md");
    fs::path newP = fs::path(notesDir()) / (clean + ".md");
    if (!fs::exists(oldP, ec) || fs::exists(newP, ec)) return false;
    fs::rename(oldP, newP, ec);
    if (ec) return false;
    if (toolbarUsageByNote_.count(noteId)) {
        toolbarUsageByNote_[clean] = std::move(toolbarUsageByNote_[noteId]);
        toolbarUsageByNote_.erase(noteId);
        savePrefs();
    }
    return true;
}

bool NotesCoordinator::deleteNote(const std::string& noteId, bool confirmed) {
    if (!confirmed || noteId.empty()) return false;
    std::error_code ec;
    fs::path p = fs::path(notesDir()) / (noteId + ".md");
    bool removed = fs::remove(p, ec);
    if (removed) {
        toolbarUsageByNote_.erase(noteId);
        savePrefs();
    }
    return removed;
}

void NotesCoordinator::recordToolbarUsage(const std::string& noteId, const std::string& actionId) {
    toolbarUsageByNote_[noteId][actionId] += 1;
    savePrefs();
}

std::vector<ToolbarAction> NotesCoordinator::toolbarActionsForNote(const std::string& noteId) const {
    auto base = NotesMarkdown::defaultToolbarActions();
    auto it = toolbarUsageByNote_.find(noteId);
    if (it == toolbarUsageByNote_.end()) return base;
    return NotesMarkdown::orderToolbarActions(base, it->second);
}

void NotesCoordinator::resetToolbarOrder(const std::string& noteId) {
    toolbarUsageByNote_.erase(noteId);
    savePrefs();
}

void NotesCoordinator::UndoStack::push(const std::string& content, size_t caretPos) {
    if (!undo_.empty() && undo_.back().content == content) return;
    undo_.push_back({content, caretPos});
    if (undo_.size() > 200) undo_.erase(undo_.begin());
    redo_.clear();
}

void NotesCoordinator::UndoStack::reset(const std::string& content, size_t caretPos) {
    undo_.clear();
    redo_.clear();
    undo_.push_back({content, caretPos});
}

std::optional<UndoSnapshot> NotesCoordinator::UndoStack::undo() {
    if (undo_.size() <= 1) return std::nullopt;
    redo_.push_back(undo_.back());
    undo_.pop_back();
    return undo_.back();
}

std::optional<UndoSnapshot> NotesCoordinator::UndoStack::redo() {
    if (redo_.empty()) return std::nullopt;
    auto snap = redo_.back();
    redo_.pop_back();
    undo_.push_back(snap);
    return snap;
}

std::vector<core::SearchResult> NotesCoordinator::query(const core::QueryContext& ctx) {
    if (!core_ || ctx.activeChip.has_value()) return {};

    std::string qLower = core::toLowerAscii(ctx.trimmedQuery);
    std::vector<core::SearchResult> results;

    // Note bang / quick open: "n" or "n <title>" or "note <title>"
    bool isNoteBang = (qLower == "n" || qLower.rfind("n ", 0) == 0 ||
                       qLower == "note" || qLower.rfind("note ", 0) == 0);
    std::string subQuery = ctx.trimmedQuery;
    if (qLower.rfind("n ", 0) == 0) subQuery = core::trimAscii(ctx.trimmedQuery.substr(2));
    else if (qLower.rfind("note ", 0) == 0) subQuery = core::trimAscii(ctx.trimmedQuery.substr(5));
    else if (qLower == "n" || qLower == "note") subQuery.clear();

    if (isNoteBang) {
        core::SearchResult createRes;
        createRes.id = "note:create";
        createRes.title = subQuery.empty() ? "New Markdown Note" : ("Create Note: " + subQuery);
        createRes.subtitle = "Open standalone Markdown note editor";
        createRes.category = core::SearchCategory::Notes;
        createRes.categoryLabel = "Note";
        createRes.score = 97.0;
        createRes.providerId = providerId();
        createRes.actionId = subQuery.empty() ? "__new__" : ("__new__:" + subQuery);
        results.push_back(std::move(createRes));
    }

    for (const auto& note : listNotes()) {
        std::string resId = "note:" + note.id;
        if (core_->usage().isResultHidden(resId)) continue;
        double sc = 0.0;
        if (isNoteBang) {
            sc = subQuery.empty() ? 85.0 : core::scoreMatch(subQuery, note.title, note.filePath);
        } else if (!ctx.trimmedQuery.empty()) {
            sc = core::scoreMatch(ctx.trimmedQuery, note.title, note.filePath);
        }
        if (sc <= 0.0) continue;

        core::SearchResult r;
        r.id = resId;
        r.title = note.title;
        r.subtitle = note.filePath;
        r.category = core::SearchCategory::Notes;
        r.categoryLabel = "Note";
        r.score = sc;
        r.providerId = providerId();
        r.actionId = note.id;
        r.canHide = true;
        results.push_back(std::move(r));
    }
    return results;
}

core::ActionResult NotesCoordinator::execute(const core::SearchResult& item) {
    if (!core_) return {false, false, "Core unavailable", "", ""};
    if (item.actionId.rfind("__new__:", 0) == 0) {
        std::string title = item.actionId.substr(8);
        auto created = createNote(title);
        if (!created) return {false, false, "Failed to create note", "", ""};
        core_->events().publish(core::OpenNoteEvent{created->id});
        return {true, true, "Created note " + created->title, "", created->id};
    }
    if (item.actionId == "__new__") {
        auto created = createNote("Quick Note");
        if (!created) return {false, false, "Failed to create note", "", ""};
        core_->events().publish(core::OpenNoteEvent{created->id});
        return {true, true, "Created note", "", created->id};
    }
    core_->events().publish(core::OpenNoteEvent{item.actionId});
    core_->usage().recordExecution(item.id, core_->platform().currentTimeMs());
    return {true, true, "Opened note " + item.title, "", item.actionId};
}

}  // namespace cmdpal::features::notes
