#pragma once

#include "core/IPlatformServices.hpp"

#include <string>
#include <vector>

namespace cmdpal::test {

class MockPlatformServices : public core::IPlatformServices {
public:
    core::KnownFolders knownFolders;
    core::Point cursor{500, 400};
    core::MonitorWorkArea primaryMonitor{{0, 0, 1920, 1040}, 1.0};
    core::MonitorWorkArea cursorMonitor{{0, 0, 1920, 1040}, 1.0};
    bool startupEnabled = false;
    bool darkMode = true;
    mutable int64_t nowMs = 10000;

    std::vector<std::string> openedUrls;
    struct LaunchedProcess {
        std::string exe;
        std::vector<std::string> args;
        std::string workingDir;
    };
    std::vector<LaunchedProcess> launchedProcesses;
    std::vector<std::string> openedShellItems;

    bool openExternalUrl(const std::string& validatedHttpUrl) override {
        openedUrls.push_back(validatedHttpUrl);
        return true;
    }

    bool launchExecutable(const std::string& exePath,
                          const std::vector<std::string>& args,
                          const std::string& workingDir = "") override {
        launchedProcesses.push_back({exePath, args, workingDir});
        return true;
    }

    bool openShellItem(const std::string& path) override {
        openedShellItems.push_back(path);
        return true;
    }

    core::KnownFolders getKnownFolders() const override { return knownFolders; }
    core::Point getCursorPosition() const override { return cursor; }
    core::MonitorWorkArea getMonitorWorkAreaAt(core::Point) const override { return cursorMonitor; }
    core::MonitorWorkArea getPrimaryMonitorWorkArea() const override { return primaryMonitor; }

    bool setStartupEnabled(bool enabled) override {
        startupEnabled = enabled;
        return true;
    }
    bool isStartupEnabled() const override { return startupEnabled; }
    bool isDarkModePreferred() const override { return darkMode; }
    int64_t currentTimeMs() const override {
        nowMs += 10;
        return nowMs;
    }
};

}  // namespace cmdpal::test
