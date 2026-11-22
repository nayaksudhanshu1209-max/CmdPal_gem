#pragma once

#include "features/notes/NotesModels.hpp"

#include <map>
#include <string>
#include <vector>

namespace cmdpal::features::notes {

enum class SpanStyle {
    Plain,
    SyntaxMarker,
    Heading1,
    Heading2,
    Heading3,
    Bold,
    Italic,
    Strikethrough,
    InlineCode,
    CodeBlock,
    Blockquote,
    ListMarker,
    TaskUnchecked,
    TaskChecked,
    LinkText,
    LinkUrl,
    HorizontalRule,
    TableRow
};

struct RenderedSpan {
    size_t sourceStart = 0;  // inclusive byte offset in Markdown source
    size_t sourceEnd = 0;    // exclusive byte offset in Markdown source
    std::string text;        // visible text (markers hidden if caret is outside)
    SpanStyle style = SpanStyle::Plain;
    bool isSyntaxMarker = false;
    bool dimmedMarker = false;
};

struct RenderedLine {
    size_t lineIndex = 0;
    size_t sourceStart = 0;
    size_t sourceEnd = 0;
    std::vector<RenderedSpan> spans;
    bool caretOnLine = false;
};

struct FormattedEditResult {
    std::string newContent;
    size_t newCaretStart = 0;
    size_t newCaretEnd = 0;
};

class NotesMarkdown {
public:
    // Renders full Markdown document into lines & spans with caret-sensitive syntax visibility.
    // When caretPos is inside a formatted region, markers are emitted with dimmedMarker=true.
    // When caretPos is outside, markers are hidden while preserving sourceStart/sourceEnd mapping.
    static std::vector<RenderedLine> renderDocument(const std::string& source, size_t caretPos);

    // Applies a toolbar formatting action to [selStart, selEnd] in source.
    static FormattedEditResult applyToolbarAction(const std::string& source,
                                                  size_t selStart,
                                                  size_t selEnd,
                                                  const ToolbarAction& action);

    // Handles Enter key syntax assistance (e.g., continuing "- ", "- [ ] ", "1. ", "> ").
    static FormattedEditResult handleEnterKey(const std::string& source, size_t caretPos);

    // Maps a source offset to visual column on its line (given current caretPos).
    static size_t sourceOffsetToVisualColumn(const std::vector<RenderedLine>& lines, size_t sourceOffset);

    // Default toolbar actions
    static std::vector<ToolbarAction> defaultToolbarActions();

    // Sorts toolbar actions by per-note usage counts (descending), keeping default order on ties.
    static std::vector<ToolbarAction> orderToolbarActions(
        const std::vector<ToolbarAction>& base,
        const std::map<std::string, int>& usageCounts);
};

}  // namespace cmdpal::features::notes
