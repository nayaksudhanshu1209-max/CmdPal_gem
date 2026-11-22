#include "features/commands/CommandsCoordinator.hpp"

#include "core/SearchMatch.hpp"

#include <algorithm>

namespace cmdpal::features::commands {

CommandsCoordinator::CommandsCoordinator(std::function<void()> onQuitRequested)
    : onQuitRequested_(std::move(onQuitRequested)) {
    builtins_ = {
        {"cmd:settings", "Open CmdPal Settings", "Configure shortcuts, search roots, web bangs, and preferences", "settings", false, ""},
        {"cmd:notes", "Open Notes", "Open the standalone Markdown note editor", "note", false, ""},
        {"cmd:new-note", "Create New Note", "Create and open a new Markdown note", "newnote", false, ""},
        {"cmd:rebuild-index", "Rebuild File Search Index", "Re-scan configured file search folders", "reindex", false, ""},
        {"cmd:reload-apps", "Reload Applications", "Refresh cached Start Menu applications", "reloadapps", false, ""},
        {"cmd:quit", "Quit CmdPal", "Exit CmdPal cleanly and stop background tasks", "quit", true,
         "Are you sure you want to exit CmdPal? The global shortcut will stop listening."}
    };
}

void CommandsCoordinator::initialize(core::CoreServices& core) {
    core_ = &core;
}

void CommandsCoordinator::shutdown() {
    core_ = nullptr;
}

std::vector<core::SettingDescriptor> CommandsCoordinator::settingDescriptors() const {
    return {
        {"commands.user", "User-Defined Commands", "Create, validate, and manage custom executable tasks",
         "commands", {"command", "task", "custom", "alias", "executable"}}
    };
}

CommandValidationResult CommandsCoordinator::validateUserCommand(const core::UserCommandDef& cmd) const {
    if (core::trimAscii(cmd.name).empty()) {
        return {false, "Command name cannot be empty."};
    }
    if (core::trimAscii(cmd.executable).empty()) {
        return {false, "Executable path cannot be empty."};
    }
    auto hasControlChars = [](const std::string& s) {
        for (unsigned char c : s) {
            if (c < 0x20) return true;
        }
        return false;
    };
    if (hasControlChars(cmd.executable) || hasControlChars(cmd.workingDir)) {
        return {false, "Paths cannot contain control characters."};
    }
    for (const auto& arg : cmd.args) {
        if (arg.find('\0') != std::string::npos) {
            return {false, "Arguments cannot contain null bytes."};
        }
    }
    return {true, ""};
}

std::vector<core::SearchResult> CommandsCoordinator::query(const core::QueryContext& ctx) {
    if (!core_) return {};
    if (ctx.activeChip.has_value()) return {};

    std::vector<core::SearchResult> results;
    std::string qLower = core::toLowerAscii(ctx.trimmedQuery);

    for (const auto& b : builtins_) {
        if (core_->usage().isResultHidden(b.id)) continue;
        double s = 0.0;
        if (ctx.trimmedQuery.empty()) {
            s = 12.0;
        } else if (!b.alias.empty() && qLower == b.alias) {
            s = 98.0;
        } else {
            s = std::max(core::scoreMatch(ctx.trimmedQuery, b.name, b.description),
                         core::scoreMatch(ctx.trimmedQuery, b.alias, ""));
        }
        if (s <= 0.0) continue;

        core::SearchResult r;
        r.id = b.id;
        r.title = b.name;
        r.subtitle = b.description;
        r.category = core::SearchCategory::Commands;
        r.categoryLabel = "Command";
        r.score = s;
        r.providerId = providerId();
        r.actionId = b.id;
        r.canHide = true;
        r.requiresConfirmation = b.requiresConfirmation;
        r.confirmationMessage = b.confirmationMessage;
        results.push_back(std::move(r));
    }

    auto settings = core_->settings().snapshot();
    for (const auto& uc : settings.userCommands) {
        if (!uc.enabled) continue;
        std::string resId = "ucmd:" + uc.id;
        if (core_->usage().isResultHidden(resId)) continue;
        if (!validateUserCommand(uc).valid) continue;

        double s = 0.0;
        if (ctx.trimmedQuery.empty()) {
            s = 14.0;
        } else if (!uc.alias.empty() && qLower == core::toLowerAscii(uc.alias)) {
            s = 99.0;
        } else {
            s = std::max(core::scoreMatch(ctx.trimmedQuery, uc.name, uc.description),
                         uc.alias.empty() ? 0.0 : core::scoreMatch(ctx.trimmedQuery, uc.alias, ""));
        }
        if (s <= 0.0) continue;

        core::SearchResult r;
        r.id = resId;
        r.title = uc.name;
        r.subtitle = uc.description.empty() ? ("Run: " + uc.executable) : uc.description;
        r.category = core::SearchCategory::Commands;
        r.categoryLabel = uc.category.empty() ? "Custom Command" : uc.category;
        r.score = s;
        r.providerId = providerId();
        r.actionId = uc.id;
        r.canHide = true;
        r.requiresConfirmation = uc.requiresConfirmation;
        if (uc.requiresConfirmation) {
            r.confirmationMessage = "Run command '" + uc.name + "' (" + uc.executable + ")?";
        }
        results.push_back(std::move(r));
    }

    return results;
}

core::ActionResult CommandsCoordinator::execute(const core::SearchResult& item) {
    if (!core_) return {false, false, "Core unavailable", "", ""};

    if (item.actionId == "cmd:settings") {
        core_->events().publish(core::OpenSettingsEvent{"general"});
        return {true, true, "Opening Settings", "general", ""};
    }
    if (item.actionId == "cmd:notes") {
        core_->events().publish(core::OpenNoteEvent{""});
        return {true, true, "Opening Notes", "", "default"};
    }
    if (item.actionId == "cmd:new-note") {
        core_->events().publish(core::OpenNoteEvent{"__new__"});
        return {true, true, "Creating new note", "", "__new__"};
    }
    if (item.actionId == "cmd:rebuild-index") {
        core_->events().publish(core::IndexStatusChangedEvent{"rebuild_requested", 0, false});
        return {true, true, "Rebuilding file index", "", ""};
    }
    if (item.actionId == "cmd:reload-apps") {
        core_->events().publish(core::AppsReloadRequestedEvent{});
        return {true, true, "Reloading applications", "", ""};
    }
    if (item.actionId == "cmd:quit") {
        if (onQuitRequested_) onQuitRequested_();
        return {true, true, "Exiting CmdPal", "", ""};
    }

    auto settings = core_->settings().snapshot();
    for (const auto& uc : settings.userCommands) {
        if (uc.id == item.actionId && uc.enabled) {
            if (!validateUserCommand(uc).valid) {
                return {false, false, "Invalid user command definition", "", ""};
            }
            bool ok = core_->platform().launchExecutable(uc.executable, uc.args, uc.workingDir);
            if (ok) {
                core_->usage().recordExecution(item.id, core_->platform().currentTimeMs());
            }
            return {ok, ok, ok ? "Executed " + uc.name : "Failed to execute " + uc.name, "", ""};
        }
    }

    return {false, false, "Unknown command", "", ""};
}

}  // namespace cmdpal::features::commands
