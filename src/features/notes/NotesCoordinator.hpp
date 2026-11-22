#pragma once

#include "core/CoreServices.hpp"
#include "features/notes/NotesContract.hpp"
#include "features/notes/NotesMarkdown.hpp"

#include <map>
#include <string>
#include <vector>

namespace cmdpal::features::notes {

class NotesCoordinator : public core::IFeature,
                         public core::ISearchProvider,
                         public INotesService {
public:
    // IFeature
    std::string id() const override { return "notes"; }
    std::string displayName() const override { return "Markdown Notes"; }
    void initialize(core::CoreServices& core) override;
    void shutdown() override;
    core::ISearchProvider* searchProvider() override { return this; }
    std::vector<core::SettingDescriptor> settingDescriptors() const override;

    // ISearchProvider
    std::string providerId() const override { return "notes"; }
    core::SearchCategory category() const override { return core::SearchCategory::Notes; }
    std::vector<core::SearchResult> query(const core::QueryContext& ctx) override;
    core::ActionResult execute(const core::SearchResult& item) override;

    // INotesService
    std::vector<NoteMetadata> listNotes() const override;
    std::optional<NoteMetadata> createNote(const std::string& title) override;
    std::string loadNoteContent(const std::string& noteId) const override;
    bool saveNoteContent(const std::string& noteId, const std::string& content) override;
    bool renameNote(const std::string& noteId, const std::string& newTitle) override;
    bool deleteNote(const std::string& noteId, bool confirmed) override;

    // Per-note toolbar ordering & preferences
    void recordToolbarUsage(const std::string& noteId, const std::string& actionId);
    std::vector<ToolbarAction> toolbarActionsForNote(const std::string& noteId) const;
    void resetToolbarOrder(const std::string& noteId);

    // Undo / Redo helper
    class UndoStack {
    public:
        void push(const std::string& content, size_t caretPos);
        bool canUndo() const { return undo_.size() > 1; }
        bool canRedo() const { return !redo_.empty(); }
        std::optional<UndoSnapshot> undo();
        std::optional<UndoSnapshot> redo();
        void reset(const std::string& content, size_t caretPos);

    private:
        std::vector<UndoSnapshot> undo_;
        std::vector<UndoSnapshot> redo_;
    };

private:
    std::string notesDir() const;
    std::string prefsPath() const;
    static std::string sanitizeFileName(const std::string& title);
    void loadPrefs();
    void savePrefs() const;

    core::CoreServices* core_ = nullptr;
    std::map<std::string, std::map<std::string, int>> toolbarUsageByNote_;
};

}  // namespace cmdpal::features::notes
