#include "features/notes/NotesMarkdown.hpp"

#include <algorithm>
#include <cctype>

namespace cmdpal::features::notes {

namespace {

void parseInlineSpans(const std::string& line,
                      size_t lineSourceBase,
                      size_t caretPos,
                      SpanStyle baseStyle,
                      std::vector<RenderedSpan>& out) {
    size_t i = 0;
    size_t n = line.size();
    size_t plainStart = 0;

    auto flushPlain = [&](size_t endIdx) {
        if (endIdx > plainStart) {
            out.push_back({lineSourceBase + plainStart,
                           lineSourceBase + endIdx,
                           line.substr(plainStart, endIdx - plainStart),
                           baseStyle,
                           false,
                           false});
        }
    };

    while (i < n) {
        // Inline code: `...`
        if (line[i] == '`') {
            size_t close = line.find('`', i + 1);
            if (close != std::string::npos && close > i + 1) {
                flushPlain(i);
                size_t absStart = lineSourceBase + i;
                size_t absEnd = lineSourceBase + close + 1;
                bool caretInside = (caretPos >= absStart && caretPos <= absEnd);
                if (caretInside) {
                    out.push_back({absStart, absStart + 1, "`", SpanStyle::SyntaxMarker, true, true});
                }
                out.push_back({absStart + 1, absEnd - 1, line.substr(i + 1, close - i - 1),
                               SpanStyle::InlineCode, false, false});
                if (caretInside) {
                    out.push_back({absEnd - 1, absEnd, "`", SpanStyle::SyntaxMarker, true, true});
                }
                i = close + 1;
                plainStart = i;
                continue;
            }
        }

        // Bold: **...**
        if (i + 1 < n && line[i] == '*' && line[i + 1] == '*') {
            size_t close = line.find("**", i + 2);
            if (close != std::string::npos && close > i + 2) {
                flushPlain(i);
                size_t absStart = lineSourceBase + i;
                size_t absEnd = lineSourceBase + close + 2;
                bool caretInside = (caretPos >= absStart && caretPos <= absEnd);
                if (caretInside) {
                    out.push_back({absStart, absStart + 2, "**", SpanStyle::SyntaxMarker, true, true});
                }
                out.push_back({absStart + 2, absEnd - 2, line.substr(i + 2, close - i - 2),
                               SpanStyle::Bold, false, false});
                if (caretInside) {
                    out.push_back({absEnd - 2, absEnd, "**", SpanStyle::SyntaxMarker, true, true});
                }
                i = close + 2;
                plainStart = i;
                continue;
            }
        }

        // Strikethrough: ~~...~~
        if (i + 1 < n && line[i] == '~' && line[i + 1] == '~') {
            size_t close = line.find("~~", i + 2);
            if (close != std::string::npos && close > i + 2) {
                flushPlain(i);
                size_t absStart = lineSourceBase + i;
                size_t absEnd = lineSourceBase + close + 2;
                bool caretInside = (caretPos >= absStart && caretPos <= absEnd);
                if (caretInside) {
                    out.push_back({absStart, absStart + 2, "~~", SpanStyle::SyntaxMarker, true, true});
                }
                out.push_back({absStart + 2, absEnd - 2, line.substr(i + 2, close - i - 2),
                               SpanStyle::Strikethrough, false, false});
                if (caretInside) {
                    out.push_back({absEnd - 2, absEnd, "~~", SpanStyle::SyntaxMarker, true, true});
                }
                i = close + 2;
                plainStart = i;
                continue;
            }
        }

        // Italic: *...*
        if (line[i] == '*' && (i + 1 >= n || line[i + 1] != '*')) {
            size_t close = line.find('*', i + 1);
            if (close != std::string::npos && close > i + 1) {
                flushPlain(i);
                size_t absStart = lineSourceBase + i;
                size_t absEnd = lineSourceBase + close + 1;
                bool caretInside = (caretPos >= absStart && caretPos <= absEnd);
                if (caretInside) {
                    out.push_back({absStart, absStart + 1, "*", SpanStyle::SyntaxMarker, true, true});
                }
                out.push_back({absStart + 1, absEnd - 1, line.substr(i + 1, close - i - 1),
                               SpanStyle::Italic, false, false});
                if (caretInside) {
                    out.push_back({absEnd - 1, absEnd, "*", SpanStyle::SyntaxMarker, true, true});
                }
                i = close + 1;
                plainStart = i;
                continue;
            }
        }

        // Link: [text](url)
        if (line[i] == '[') {
            size_t mid = line.find("](", i + 1);
            if (mid != std::string::npos) {
                size_t close = line.find(')', mid + 2);
                if (close != std::string::npos) {
                    flushPlain(i);
                    size_t absStart = lineSourceBase + i;
                    size_t absMid = lineSourceBase + mid;
                    size_t absEnd = lineSourceBase + close + 1;
                    bool caretInside = (caretPos >= absStart && caretPos <= absEnd);
                    if (caretInside) {
                        out.push_back({absStart, absStart + 1, "[", SpanStyle::SyntaxMarker, true, true});
                    }
                    out.push_back({absStart + 1, absMid, line.substr(i + 1, mid - i - 1),
                                   SpanStyle::LinkText, false, false});
                    if (caretInside) {
                        out.push_back({absMid, absMid + 2, "](", SpanStyle::SyntaxMarker, true, true});
                        out.push_back({absMid + 2, absEnd - 1, line.substr(mid + 2, close - mid - 2),
                                       SpanStyle::LinkUrl, false, true});
                        out.push_back({absEnd - 1, absEnd, ")", SpanStyle::SyntaxMarker, true, true});
                    }
                    i = close + 1;
                    plainStart = i;
                    continue;
                }
            }
        }

        ++i;
    }

    flushPlain(n);
}

}  // namespace

std::vector<RenderedLine> NotesMarkdown::renderDocument(const std::string& source, size_t caretPos) {
    std::vector<RenderedLine> lines;
    size_t pos = 0;
    size_t lineIdx = 0;
    bool inCodeFence = false;

    while (pos <= source.size()) {
        size_t nl = source.find('\n', pos);
        size_t end = (nl == std::string::npos) ? source.size() : nl;
        std::string rawLine = source.substr(pos, end - pos);
        if (!rawLine.empty() && rawLine.back() == '\r') rawLine.pop_back();

        RenderedLine rl;
        rl.lineIndex = lineIdx++;
        rl.sourceStart = pos;
        rl.sourceEnd = end;
        rl.caretOnLine = (caretPos >= pos && caretPos <= end);

        if (rawLine.rfind("```", 0) == 0) {
            inCodeFence = !inCodeFence;
            rl.spans.push_back({pos, end, rawLine, SpanStyle::SyntaxMarker, true, rl.caretOnLine});
        } else if (inCodeFence) {
            rl.spans.push_back({pos, end, rawLine, SpanStyle::CodeBlock, false, false});
        } else if (rawLine == "---" || rawLine == "***" || rawLine == "___") {
            rl.spans.push_back({pos, end, rl.caretOnLine ? rawLine : "────────────────────",
                                SpanStyle::HorizontalRule, false, rl.caretOnLine});
        } else if (rawLine.rfind("### ", 0) == 0) {
            if (rl.caretOnLine) {
                rl.spans.push_back({pos, pos + 4, "### ", SpanStyle::SyntaxMarker, true, true});
            }
            parseInlineSpans(rawLine.substr(4), pos + 4, caretPos, SpanStyle::Heading3, rl.spans);
        } else if (rawLine.rfind("## ", 0) == 0) {
            if (rl.caretOnLine) {
                rl.spans.push_back({pos, pos + 3, "## ", SpanStyle::SyntaxMarker, true, true});
            }
            parseInlineSpans(rawLine.substr(3), pos + 3, caretPos, SpanStyle::Heading2, rl.spans);
        } else if (rawLine.rfind("# ", 0) == 0) {
            if (rl.caretOnLine) {
                rl.spans.push_back({pos, pos + 2, "# ", SpanStyle::SyntaxMarker, true, true});
            }
            parseInlineSpans(rawLine.substr(2), pos + 2, caretPos, SpanStyle::Heading1, rl.spans);
        } else if (rawLine.rfind("> ", 0) == 0) {
            rl.spans.push_back({pos, pos + 2, rl.caretOnLine ? "> " : "│ ",
                                SpanStyle::Blockquote, true, rl.caretOnLine});
            parseInlineSpans(rawLine.substr(2), pos + 2, caretPos, SpanStyle::Blockquote, rl.spans);
        } else if (rawLine.rfind("- [ ] ", 0) == 0) {
            rl.spans.push_back({pos, pos + 6, rl.caretOnLine ? "- [ ] " : "☐ ",
                                SpanStyle::TaskUnchecked, true, rl.caretOnLine});
            parseInlineSpans(rawLine.substr(6), pos + 6, caretPos, SpanStyle::Plain, rl.spans);
        } else if (rawLine.rfind("- [x] ", 0) == 0 || rawLine.rfind("- [X] ", 0) == 0) {
            rl.spans.push_back({pos, pos + 6, rl.caretOnLine ? rawLine.substr(0, 6) : "☑ ",
                                SpanStyle::TaskChecked, true, rl.caretOnLine});
            parseInlineSpans(rawLine.substr(6), pos + 6, caretPos, SpanStyle::Strikethrough, rl.spans);
        } else if (rawLine.rfind("- ", 0) == 0 || rawLine.rfind("* ", 0) == 0) {
            rl.spans.push_back({pos, pos + 2, rl.caretOnLine ? rawLine.substr(0, 2) : "• ",
                                SpanStyle::ListMarker, true, rl.caretOnLine});
            parseInlineSpans(rawLine.substr(2), pos + 2, caretPos, SpanStyle::Plain, rl.spans);
        } else if (!rawLine.empty() && rawLine[0] == '|' && rawLine.back() == '|') {
            parseInlineSpans(rawLine, pos, caretPos, SpanStyle::TableRow, rl.spans);
        } else {
            parseInlineSpans(rawLine, pos, caretPos, SpanStyle::Plain, rl.spans);
        }

        lines.push_back(std::move(rl));
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return lines;
}

FormattedEditResult NotesMarkdown::applyToolbarAction(const std::string& source,
                                                      size_t selStart,
                                                      size_t selEnd,
                                                      const ToolbarAction& action) {
    if (selStart > selEnd) std::swap(selStart, selEnd);
    selStart = std::min(selStart, source.size());
    selEnd = std::min(selEnd, source.size());

    FormattedEditResult res;
    if (action.blockLinePrefix) {
        size_t lineStart = source.rfind('\n', selStart == 0 ? 0 : selStart - 1);
        lineStart = (lineStart == std::string::npos || selStart == 0) ? 0 : lineStart + 1;
        res.newContent = source.substr(0, lineStart) + action.prefix + source.substr(lineStart);
        res.newCaretStart = selStart + action.prefix.size();
        res.newCaretEnd = selEnd + action.prefix.size();
        return res;
    }

    std::string selected = source.substr(selStart, selEnd - selStart);
    std::string replacement = action.prefix + selected + action.suffix;
    res.newContent = source.substr(0, selStart) + replacement + source.substr(selEnd);
    if (selected.empty()) {
        res.newCaretStart = selStart + action.prefix.size();
        res.newCaretEnd = res.newCaretStart;
    } else {
        res.newCaretStart = selStart + action.prefix.size();
        res.newCaretEnd = res.newCaretStart + selected.size();
    }
    return res;
}

FormattedEditResult NotesMarkdown::handleEnterKey(const std::string& source, size_t caretPos) {
    caretPos = std::min(caretPos, source.size());
    size_t lineStart = source.rfind('\n', caretPos == 0 ? 0 : caretPos - 1);
    lineStart = (lineStart == std::string::npos || caretPos == 0) ? 0 : lineStart + 1;
    std::string line = source.substr(lineStart, caretPos - lineStart);

    std::string continuation;
    if (line.rfind("- [ ] ", 0) == 0 || line.rfind("- [x] ", 0) == 0) {
        if (line.size() > 6) continuation = "- [ ] ";
    } else if (line.rfind("- ", 0) == 0) {
        if (line.size() > 2) continuation = "- ";
    } else if (line.rfind("* ", 0) == 0) {
        if (line.size() > 2) continuation = "* ";
    } else if (line.rfind("> ", 0) == 0) {
        if (line.size() > 2) continuation = "> ";
    }

    FormattedEditResult res;
    std::string ins = "\n" + continuation;
    res.newContent = source.substr(0, caretPos) + ins + source.substr(caretPos);
    res.newCaretStart = caretPos + ins.size();
    res.newCaretEnd = res.newCaretStart;
    return res;
}

size_t NotesMarkdown::sourceOffsetToVisualColumn(const std::vector<RenderedLine>& lines, size_t sourceOffset) {
    for (const auto& line : lines) {
        if (sourceOffset >= line.sourceStart && sourceOffset <= line.sourceEnd) {
            size_t col = 0;
            for (const auto& sp : line.spans) {
                if (sourceOffset <= sp.sourceStart) break;
                if (sourceOffset >= sp.sourceEnd) {
                    col += sp.text.size();
                } else {
                    size_t delta = sourceOffset - sp.sourceStart;
                    col += std::min(delta, sp.text.size());
                    break;
                }
            }
            return col;
        }
    }
    return 0;
}

std::vector<ToolbarAction> NotesMarkdown::defaultToolbarActions() {
    return {
        {"bold", "B", "Bold (**text**, Ctrl+B)", "**", "**", false, 0},
        {"italic", "I", "Italic (*text*, Ctrl+I)", "*", "*", false, 1},
        {"strike", "S", "Strikethrough (~~text~~)", "~~", "~~", false, 2},
        {"code", "</>", "Inline code (`code`, Ctrl+E)", "`", "`", false, 3},
        {"h1", "H1", "Heading 1 (# )", "# ", "", true, 4},
        {"h2", "H2", "Heading 2 (## )", "## ", "", true, 5},
        {"bullet", "• List", "Bullet list (- )", "- ", "", true, 6},
        {"task", "☐ Task", "Task list (- [ ] )", "- [ ] ", "", true, 7},
        {"quote", "Quote", "Blockquote (> )", "> ", "", true, 8},
        {"link", "Link", "Link ([text](url), Ctrl+K)", "[", "](https://)", false, 9},
        {"codeblock", "Code Block", "Fenced code block (```)", "```\n", "\n```", false, 10},
        {"hr", "Rule", "Horizontal rule (---)", "\n---\n", "", false, 11}
    };
}

std::vector<ToolbarAction> NotesMarkdown::orderToolbarActions(
    const std::vector<ToolbarAction>& base,
    const std::map<std::string, int>& usageCounts) {
    auto copy = base;
    std::stable_sort(copy.begin(), copy.end(), [&](const ToolbarAction& a, const ToolbarAction& b) {
        int ca = 0, cb = 0;
        if (auto it = usageCounts.find(a.id); it != usageCounts.end()) ca = it->second;
        if (auto it = usageCounts.find(b.id); it != usageCounts.end()) cb = it->second;
        if (ca != cb) return ca > cb;
        return a.defaultOrder < b.defaultOrder;
    });
    return copy;
}

}  // namespace cmdpal::features::notes
