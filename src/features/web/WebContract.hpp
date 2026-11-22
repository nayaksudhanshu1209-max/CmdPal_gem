#pragma once

#include "core/Contracts.hpp"
#include "core/SettingsModels.hpp"

#include <optional>
#include <string>
#include <vector>

namespace cmdpal::features::web {

struct ParsedWebInput {
    std::optional<core::AliasChip> chip;
    std::string remainingQuery;
};

struct UrlValidationResult {
    bool isUrl = false;
    bool rejectedDangerousScheme = false;
    std::string normalizedUrl;
    std::string reason;
};

class IWebService {
public:
    virtual ~IWebService() = default;
    virtual ParsedWebInput parseInput(const std::string& rawQuery) const = 0;
    virtual UrlValidationResult validateAndNormalizeUrl(const std::string& input) const = 0;
};

}  // namespace cmdpal::features::web
