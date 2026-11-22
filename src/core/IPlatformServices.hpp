#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cmdpal::core {

struct Point {
    int x = 0;
    int y = 0;
};

struct Rect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    int width() const { return right - left; }
    int height() const { return bottom - top; }
};

struct MonitorWorkArea {
    Rect workArea;
    double dpiScale = 1.0;
};

struct KnownFolders {
    std::string desktop;
    std::string documents;
    std::string downloads;
    std::string appData;
    std::string startMenuUser;
    std::string startMenuCommon;
};

class IPlatformServices {
public:
    virtual ~IPlatformServices() = default;

    // External URL & process/shell execution
    virtual bool openExternalUrl(const std::string& validatedHttpUrl) = 0;
    virtual bool launchExecutable(const std::string& exePath,
                                  const std::vector<std::string>& args,
                                  const std::string& workingDir = "") = 0;
    virtual bool openShellItem(const std::string& path) = 0;

    // System paths & environment
    virtual KnownFolders getKnownFolders() const = 0;
    virtual Point getCursorPosition() const = 0;
    virtual MonitorWorkArea getMonitorWorkAreaAt(Point pt) const = 0;
    virtual MonitorWorkArea getPrimaryMonitorWorkArea() const = 0;

    // Startup registration (HKCU Run)
    virtual bool setStartupEnabled(bool enabled) = 0;
    virtual bool isStartupEnabled() const = 0;

    // Theme & timing
    virtual bool isDarkModePreferred() const = 0;
    virtual int64_t currentTimeMs() const = 0;
};

}  // namespace cmdpal::core
