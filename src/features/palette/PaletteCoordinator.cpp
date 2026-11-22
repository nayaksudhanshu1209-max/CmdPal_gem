#include "features/palette/PaletteCoordinator.hpp"

#include "core/SearchMatch.hpp"

#include <algorithm>

namespace cmdpal::features::palette {

void PaletteCoordinator::initialize(core::CoreServices& core) {
    core_ = &core;
    core_->events().subscribe<core::PaletteHideRequestedEvent>(
        [this](const core::PaletteHideRequestedEvent&) { closePalette(); });
}

void PaletteCoordinator::shutdown() {
    providers_.clear();
    core_ = nullptr;
}

std::vector<core::SettingDescriptor> PaletteCoordinator::settingDescriptors() const {
    return {
        {"palette.shortcut", "Global Palette Shortcut", "Keyboard shortcut to open and close CmdPal",
         "general", {"shortcut", "hotkey", "keyboard", "open"}},
        {"palette.placement", "Palette Placement & Cursor Positioning", "Center, Top-Center, or Near Mouse Cursor inside monitor work area",
         "appearance", {"cursor", "position", "placement", "monitor", "center"}},
        {"palette.density", "Display Density", "Compact or Comfortable result row density",
         "appearance", {"compact", "comfortable", "density"}},
        {"palette.theme", "Theme", "System, Light, or Dark appearance",
         "appearance", {"theme", "dark", "light", "system"}},
        {"palette.animations", "Animations", "Enable or disable restrained open/close animations",
         "appearance", {"animation", "animations", "motion"}},
        {"palette.suggestions", "Suggestions Behavior", "Show or hide suggestions on opening and while typing",
         "categories", {"suggestions", "open", "typing"}}
    };
}

void PaletteCoordinator::registerProvider(core::ISearchProvider* provider) {
    if (provider) providers_.push_back(provider);
}

void PaletteCoordinator::setBangParser(BangParserFn parser) {
    bangParser_ = std::move(parser);
}

void PaletteCoordinator::openPalette() {
    state_.isOpen = true;
    state_.rawQuery.clear();
    state_.activeChip.reset();
    state_.selectedIndex = 0;
    updateQuery("", true);
}

void PaletteCoordinator::closePalette() {
    state_.isOpen = false;
    state_.results.clear();
    state_.selectedIndex = 0;
    state_.statusMessage.clear();
}

void PaletteCoordinator::togglePalette() {
    if (state_.isOpen) closePalette();
    else openPalette();
}

void PaletteCoordinator::updateQuery(const std::string& rawQuery, bool isOpening) {
    // Always preserve rawQuery verbatim
    state_.rawQuery = rawQuery;
    state_.selectedIndex = 0;
    state_.statusMessage.clear();

    if (!core_) return;
    auto settings = core_->settings().snapshot();

    if (isOpening && !settings.showSuggestionsOnOpen) {
        state_.results.clear();
        state_.activeChip.reset();
        return;
    }
    if (!isOpening && !rawQuery.empty() && !settings.showSuggestionsWhileTyping) {
        state_.results.clear();
        return;
    }

    core::QueryContext ctx;
    ctx.rawQuery = rawQuery;
    ctx.isOpeningEmpty = core::trimAscii(rawQuery).empty();

    if (bangParser_) {
        auto parsed = bangParser_(rawQuery);
        ctx.activeChip = parsed.first;
        ctx.trimmedQuery = core::trimAscii(parsed.second);
        state_.activeChip = parsed.first;
    } else {
        ctx.trimmedQuery = core::trimAscii(rawQuery);
        state_.activeChip.reset();
    }

    std::vector<core::SearchResult> aggregated;
    for (auto* provider : providers_) {
        if (!provider) continue;
        std::string catKey = core::categoryToString(provider->category());
        auto catIt = settings.enabledCategories.find(catKey);
        if (catIt != settings.enabledCategories.end() && !catIt->second) {
            continue;
        }
        auto partial = provider->query(ctx);
        for (auto& item : partial) {
            if (core_->usage().isResultHidden(item.id)) continue;
            if (item.isApp && core_->usage().isAppHidden(item.id)) continue;
            auto itemCatIt = settings.enabledCategories.find(core::categoryToString(item.category));
            if (itemCatIt != settings.enabledCategories.end() && !itemCatIt->second) continue;

            item.score += core_->usage().frequencyBonus(item.id);
            aggregated.push_back(std::move(item));
        }
    }

    std::stable_sort(aggregated.begin(), aggregated.end(),
                     [](const core::SearchResult& a, const core::SearchResult& b) {
                         return a.score > b.score;
                     });

    if (aggregated.size() > static_cast<size_t>(settings.maxResults)) {
        aggregated.resize(static_cast<size_t>(settings.maxResults));
    }

    state_.results = std::move(aggregated);
    if (state_.results.empty() && !ctx.trimmedQuery.empty()) {
        state_.statusMessage = "No matching results for \"" + ctx.trimmedQuery + "\"";
    }
}

core::ActionResult PaletteCoordinator::handleKey(NavKey key) {
    const int count = static_cast<int>(state_.results.size());
    switch (key) {
        case NavKey::Escape:
            closePalette();
            return {true, true, "Closed palette", "", ""};
        case NavKey::Up:
            if (count > 0) {
                state_.selectedIndex = (state_.selectedIndex - 1 + count) % count;
            }
            return {true, false, "", "", ""};
        case NavKey::Down:
        case NavKey::Tab:
            if (count > 0) {
                state_.selectedIndex = (state_.selectedIndex + 1) % count;
            }
            return {true, false, "", "", ""};
        case NavKey::ShiftTab:
            if (count > 0) {
                state_.selectedIndex = (state_.selectedIndex - 1 + count) % count;
            }
            return {true, false, "", "", ""};
        case NavKey::PageUp:
            if (count > 0) {
                state_.selectedIndex = std::max(0, state_.selectedIndex - 5);
            }
            return {true, false, "", "", ""};
        case NavKey::PageDown:
            if (count > 0) {
                state_.selectedIndex = std::min(count - 1, state_.selectedIndex + 5);
            }
            return {true, false, "", "", ""};
        case NavKey::Enter:
            return activateIndex(state_.selectedIndex);
    }
    return {false, false, "", "", ""};
}

core::ActionResult PaletteCoordinator::activateIndex(int index) {
    if (index < 0 || index >= static_cast<int>(state_.results.size())) {
        return {false, false, "No result selected", "", ""};
    }
    const auto item = state_.results[static_cast<size_t>(index)];
    for (auto* provider : providers_) {
        if (provider && provider->providerId() == item.providerId) {
            auto res = provider->execute(item);
            if (res.hidePalette) {
                closePalette();
            }
            return res;
        }
    }
    return {false, false, "Provider not found", "", ""};
}

void PaletteCoordinator::hideResultAt(int index) {
    if (!core_ || index < 0 || index >= static_cast<int>(state_.results.size())) return;
    const auto& item = state_.results[static_cast<size_t>(index)];
    if (!item.canHide) return;
    if (item.isApp) {
        core_->usage().hideApp(item.id);
    } else {
        core_->usage().hideResult(item.id);
    }
    updateQuery(state_.rawQuery, false);
}

core::Rect PaletteCoordinator::computeBounds(core::PalettePlacement placement,
                                             const core::MonitorWorkArea& monitor,
                                             core::Point cursor,
                                             int desiredWidth,
                                             int desiredHeight) {
    const auto& wa = monitor.workArea;
    int w = std::min(desiredWidth, std::max(200, wa.width()));
    int h = std::min(desiredHeight, std::max(120, wa.height()));

    int x = wa.left + (wa.width() - w) / 2;
    int y = wa.top + (wa.height() - h) / 2;

    if (placement == core::PalettePlacement::TopCenter) {
        y = wa.top + std::max(24, wa.height() / 6);
    } else if (placement == core::PalettePlacement::NearCursor) {
        x = cursor.x - w / 2;
        y = cursor.y + 18;
    }

    // Clamp completely inside monitor usable work area
    if (x < wa.left) x = wa.left;
    if (y < wa.top) y = wa.top;
    if (x + w > wa.right) x = std::max(wa.left, wa.right - w);
    if (y + h > wa.bottom) y = std::max(wa.top, wa.bottom - h);

    return {x, y, x + w, y + h};
}

}  // namespace cmdpal::features::palette
