#pragma once

#include <string>
#include <vector>

namespace cmdpal::features::apps {

struct AppEntry {
    std::string id;          // Stable ID, e.g. "app:notepad"
    std::string name;        // Display name
    std::string launchPath;  // Full path to .lnk or .exe
    std::string sourceType;  // "StartMenu" or "AppPath"
};

}  // namespace cmdpal::features::apps
