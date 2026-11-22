#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cmdpal::core {

enum class SearchCategory {
    Apps,
    Commands,
    Web,
    Bookmarks,
    QuickFolders,
    Files,
    Settings,
    Notes
};

inline std::string categoryToString(SearchCategory c) {
    switch (c) {
        case SearchCategory::Apps: return "apps";
        case SearchCategory::Commands: return "commands";
        case SearchCategory::Web: return "web";
        case SearchCategory::Bookmarks: return "bookmarks";
        case SearchCategory::QuickFolders: return "quickfolders";
        case SearchCategory::Files: return "files";
        case SearchCategory::Settings: return "settings";
        case SearchCategory::Notes: return "notes";
    }
    return "unknown";
}

inline std::string categoryDisplayName(SearchCategory c) {
    switch (c) {
        case SearchCategory::Apps: return "Applications";
        case SearchCategory::Commands: return "Commands";
        case SearchCategory::Web: return "Web Search";
        case SearchCategory::Bookmarks: return "Bookmarks";
        case SearchCategory::QuickFolders: return "Quick Folders";
        case SearchCategory::Files: return "Files";
        case SearchCategory::Settings: return "Settings";
        case SearchCategory::Notes: return "Notes";
    }
    return "Other";
}

struct AliasChip {
    std::string alias;
    std::string label;
    std::string destinationId;
};

struct QueryContext {
    std::string rawQuery;
    std::string trimmedQuery;
    std::optional<AliasChip> activeChip;
    bool isOpeningEmpty = false;
};

struct SearchResult {
    std::string id;
    std::string title;
    std::string subtitle;
    SearchCategory category = SearchCategory::Commands;
    std::string categoryLabel;
    std::string iconPath;
    double score = 0.0;
    std::string providerId;
    std::string actionId;
    bool canHide = false;
    bool isApp = false;
    bool requiresConfirmation = false;
    std::string confirmationMessage;
    std::map<std::string, std::string> metadata;
};

struct ActionResult {
    bool success = false;
    bool hidePalette = true;
    std::string message;
    std::string openSettingsSection;
    std::string openNoteId;
};

struct SettingDescriptor {
    std::string key;
    std::string title;
    std::string description;
    std::string section;
    std::vector<std::string> keywords;
};

class ISearchProvider {
public:
    virtual ~ISearchProvider() = default;
    virtual std::string providerId() const = 0;
    virtual SearchCategory category() const = 0;
    virtual std::vector<SearchResult> query(const QueryContext& ctx) = 0;
    virtual ActionResult execute(const SearchResult& item) = 0;
};

class CoreServices;

class IFeature {
public:
    virtual ~IFeature() = default;
    virtual std::string id() const = 0;
    virtual std::string displayName() const = 0;
    virtual void initialize(CoreServices& core) = 0;
    virtual void shutdown() = 0;
    virtual ISearchProvider* searchProvider() { return nullptr; }
    virtual std::vector<SettingDescriptor> settingDescriptors() const { return {}; }
};

}  // namespace cmdpal::core
