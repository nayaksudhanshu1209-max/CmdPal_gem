#pragma once

#include "core/CoreServices.hpp"
#include "features/palette/PaletteContract.hpp"

#include <string>
#include <vector>

namespace cmdpal::features::palette {

class PaletteCoordinator : public core::IFeature,
                           public IPaletteService {
public:
    // IFeature
    std::string id() const override { return "palette"; }
    std::string displayName() const override { return "Command Palette"; }
    void initialize(core::CoreServices& core) override;
    void shutdown() override;
    std::vector<core::SettingDescriptor> settingDescriptors() const override;

    // IPaletteService
    void registerProvider(core::ISearchProvider* provider) override;
    void setBangParser(BangParserFn parser) override;
    void openPalette() override;
    void closePalette() override;
    void togglePalette() override;
    const PaletteState& state() const override { return state_; }

    // Search & Navigation
    void updateQuery(const std::string& rawQuery, bool isOpening = false);
    core::ActionResult handleKey(NavKey key);
    core::ActionResult activateIndex(int index);
    void hideResultAt(int index);

    // Geometry helper (multi-monitor, DPI & work-area clamped)
    static core::Rect computeBounds(core::PalettePlacement placement,
                                    const core::MonitorWorkArea& monitor,
                                    core::Point cursor,
                                    int desiredWidth,
                                    int desiredHeight);

private:
    core::CoreServices* core_ = nullptr;
    std::vector<core::ISearchProvider*> providers_;
    BangParserFn bangParser_;
    PaletteState state_;
};

}  // namespace cmdpal::features::palette
