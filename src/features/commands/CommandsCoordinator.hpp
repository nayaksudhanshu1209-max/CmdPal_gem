#pragma once

#include "core/CoreServices.hpp"
#include "features/commands/CommandsContract.hpp"

#include <functional>
#include <string>
#include <vector>

namespace cmdpal::features::commands {

class CommandsCoordinator : public core::IFeature,
                            public core::ISearchProvider,
                            public ICommandsService {
public:
    explicit CommandsCoordinator(std::function<void()> onQuitRequested = nullptr);

    // IFeature
    std::string id() const override { return "commands"; }
    std::string displayName() const override { return "Commands & Tasks"; }
    void initialize(core::CoreServices& core) override;
    void shutdown() override;
    core::ISearchProvider* searchProvider() override { return this; }
    std::vector<core::SettingDescriptor> settingDescriptors() const override;

    // ISearchProvider
    std::string providerId() const override { return "commands"; }
    core::SearchCategory category() const override { return core::SearchCategory::Commands; }
    std::vector<core::SearchResult> query(const core::QueryContext& ctx) override;
    core::ActionResult execute(const core::SearchResult& item) override;

    // ICommandsService
    CommandValidationResult validateUserCommand(const core::UserCommandDef& cmd) const override;

private:
    struct BuiltinCommand {
        std::string id;
        std::string name;
        std::string description;
        std::string alias;
        bool requiresConfirmation = false;
        std::string confirmationMessage;
    };

    core::CoreServices* core_ = nullptr;
    std::function<void()> onQuitRequested_;
    std::vector<BuiltinCommand> builtins_;
};

}  // namespace cmdpal::features::commands
