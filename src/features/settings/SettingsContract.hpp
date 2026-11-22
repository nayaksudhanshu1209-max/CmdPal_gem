#pragma once

#include <string>

namespace cmdpal::features::settings {

class ISettingsUiService {
public:
    virtual ~ISettingsUiService() = default;
    virtual void openSettings(const std::string& section = "general") = 0;
};

}  // namespace cmdpal::features::settings
