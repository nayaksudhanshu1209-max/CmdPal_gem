#pragma once

#include "features/notes/NotesModels.hpp"

#include <optional>
#include <string>
#include <vector>

namespace cmdpal::features::notes {

class INotesService {
public:
    virtual ~INotesService() = default;
    virtual std::vector<NoteMetadata> listNotes() const = 0;
    virtual std::optional<NoteMetadata> createNote(const std::string& title) = 0;
    virtual std::string loadNoteContent(const std::string& noteId) const = 0;
    virtual bool saveNoteContent(const std::string& noteId, const std::string& content) = 0;
    virtual bool renameNote(const std::string& noteId, const std::string& newTitle) = 0;
    virtual bool deleteNote(const std::string& noteId, bool confirmed) = 0;
};

}  // namespace cmdpal::features::notes
