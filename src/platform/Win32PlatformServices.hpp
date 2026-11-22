#pragma once

#include "core/IPlatformServices.hpp"

#ifdef _WIN32

namespace cmdpal::platform {

class Win32PlatformServices : public core::IPlatformServices {
public:
    bool openExternalUrl(const std::string& validatedHttpUrl) override;
    bool launchExecutable(const std::string& exePath,
                          const std::vector<std::string>& args,
                          const std::string& workingDir = "") override;
    bool openShellItem(const std::string& path) override;

    core::KnownFolders getKnownFolders() const override;
    core::Point getCursorPosition() const override;
    core::MonitorWorkArea getMonitorWorkAreaAt(core::Point pt) const override;
    core::MonitorWorkArea getPrimaryMonitorWorkArea() const override;

    bool setStartupEnabled(bool enabled) override;
    bool isStartupEnabled() const override;

    bool isDarkModePreferred() const override;
    int64_t currentTimeMs() const override;

    static std::wstring utf8ToWide(const std::string& s);
    static std::string wideToUtf8(const std::wstring& w);
    static std::string getAppDataDirectory();
};

}  // namespace cmdpal::platform

#endif  // _WIN32
