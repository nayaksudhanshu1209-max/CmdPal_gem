#pragma once

#include "core/SettingsModels.hpp"

#include <string>

namespace cmdpal::features::commands {

struct CommandValidationResult {
    bool valid = false;
    std::string error;
};

class ICommandsService {
public:
    virtual ~ICommandsService() = default;
    virtual CommandValidationResult validateUserCommand(const core::UserCommandDef& cmd) const = 0;
};

}  // namespace cmdpal::features::commands
