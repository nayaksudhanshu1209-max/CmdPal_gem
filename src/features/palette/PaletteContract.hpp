#pragma once

#include "core/Contracts.hpp"
#include "features/palette/PaletteModels.hpp"

#include <functional>
#include <optional>
#include <string>

namespace cmdpal::features::palette {

using BangParserFn = std::function<std::pair<std::optional<core::AliasChip>, std::string>(const std::string&)>;

class IPaletteService {
public:
    virtual ~IPaletteService() = default;
    virtual void registerProvider(core::ISearchProvider* provider) = 0;
    virtual void setBangParser(BangParserFn parser) = 0;
    virtual void openPalette() = 0;
    virtual void closePalette() = 0;
    virtual void togglePalette() = 0;
    virtual const PaletteState& state() const = 0;
};

}  // namespace cmdpal::features::palette
