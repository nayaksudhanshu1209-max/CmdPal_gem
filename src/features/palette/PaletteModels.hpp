#pragma once

#include "core/Contracts.hpp"
#include "core/IPlatformServices.hpp"
#include "core/SettingsModels.hpp"

#include <optional>
#include <string>
#include <vector>

namespace cmdpal::features::palette {

enum class NavKey {
    Up,
    Down,
    Tab,
    ShiftTab,
    PageUp,
    PageDown,
    Enter,
    Escape
};

struct PaletteState {
    bool isOpen = false;
    bool isLoading = false;
    std::string rawQuery;
    std::optional<core::AliasChip> activeChip;
    std::vector<core::SearchResult> results;
    int selectedIndex = 0;
    std::string statusMessage;
};

}  // namespace cmdpal::features::palette
