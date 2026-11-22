#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cmdpal::features::notes {

struct NoteMetadata {
    std::string id;        // filename stem or relative id
    std::string title;     // display title
    std::string filePath;  // path to .md file
    int64_t modifiedSec = 0;
};

struct ToolbarAction {
    std::string id;        // e.g. "bold", "italic", "h1", "code", "task", "link"
    std::string label;     // e.g. "B", "I", "H1", "</>", "[ ]", "Link"
    std::string hint;      // e.g. "**text** (Ctrl+B)"
    std::string prefix;
    std::string suffix;
    bool blockLinePrefix = false;
    int defaultOrder = 0;
};

struct UndoSnapshot {
    std::string content;
    size_t caretPos = 0;
};

}  // namespace cmdpal::features::notes
