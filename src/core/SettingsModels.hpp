#pragma once

#include <map>
#include <string>
#include <vector>

namespace cmdpal::core {

enum class DensityMode {
    Compact,
    Comfortable
};

enum class ThemeMode {
    System,
    Light,
    Dark
};

enum class PalettePlacement {
    Center,
    TopCenter,
    NearCursor
};

struct ShortcutConfig {
    bool ctrl = true;
    bool alt = true;
    bool shift = false;
    bool win = false;
    std::string key = "Space";  // e.g. "Space", "P", "K"
};

struct WebBangRule {
    std::string id;
    std::string alias;        // e.g. "g", "yt", "w"
    std::string label;        // e.g. "Google Search", "YouTube"
    std::string urlTemplate;  // e.g. "https://www.google.com/search?q={query}"
    bool enabled = true;
};

struct BookmarkItem {
    std::string id;
    std::string title;
    std::string url;
    std::string bangId;  // optional link to a bang destination, empty = general
};

struct UserCommandDef {
    std::string id;
    std::string name;
    std::string description;
    std::string alias;
    std::string category = "Custom";
    std::string executable;
    std::vector<std::string> args;
    std::string workingDir;
    bool requiresConfirmation = false;
    bool enabled = true;
};

struct QuickFolderItem {
    std::string id;
    std::string label;
    std::string path;
    bool enabled = true;
    bool indexAsRoot = false;  // explicit user-controlled link to file indexing
};

struct FileRootItem {
    std::string id;
    std::string label;
    std::string path;
    bool isDefault = false;
    bool enabled = true;
    int maxDepth = 8;
};

struct AppSettings {
    int schemaVersion = 1;

    // General & Window
    ShortcutConfig globalShortcut;
    bool runAtStartup = false;
    DensityMode density = DensityMode::Compact;
    ThemeMode theme = ThemeMode::System;
    PalettePlacement placement = PalettePlacement::Center;
    bool animationsEnabled = true;
    int animationDurationMs = 90;

    // Suggestions & Categories
    bool showSuggestionsOnOpen = true;
    bool showSuggestionsWhileTyping = true;
    int maxResults = 12;
    std::map<std::string, bool> enabledCategories = {
        {"apps", true},
        {"commands", true},
        {"web", true},
        {"bookmarks", true},
        {"quickfolders", true},
        {"files", true},
        {"settings", true},
        {"notes", true}
    };

    // Web bangs & bookmarks
    std::string defaultWebSearchId = "google";
    std::vector<WebBangRule> webBangs;
    std::vector<BookmarkItem> bookmarks;

    // User-defined commands
    std::vector<UserCommandDef> userCommands;

    // Quick Folders
    std::vector<QuickFolderItem> quickFolders;

    // File Search & Indexing
    std::vector<FileRootItem> fileRoots;
    std::vector<std::string> excludedFolderNames = {
        ".git", "node_modules", ".vs", "obj", "bin", "__pycache__", "$RECYCLE.BIN", "System Volume Information"
    };
    bool indexFileContentsOptIn = false;  // Disabled by default per spec

    // Notes
    std::string notesDirectory;
    int noteAutosaveDebounceMs = 600;
};

}  // namespace cmdpal::core
