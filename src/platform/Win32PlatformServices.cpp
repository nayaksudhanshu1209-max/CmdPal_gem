#include "platform/Win32PlatformServices.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <chrono>
#include <filesystem>

namespace cmdpal::platform {

std::wstring Win32PlatformServices::utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring w(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), needed);
    return w;
}

std::string Win32PlatformServices::wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string s(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), needed, nullptr, nullptr);
    return s;
}

std::string Win32PlatformServices::getAppDataDirectory() {
    PWSTR path = nullptr;
    std::string res = "CmdPalData";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path)) && path) {
        res = (std::filesystem::path(wideToUtf8(path)) / "CmdPal").string();
        CoTaskMemFree(path);
    }
    return res;
}

bool Win32PlatformServices::openExternalUrl(const std::string& validatedHttpUrl) {
    if (validatedHttpUrl.rfind("https://", 0) != 0 && validatedHttpUrl.rfind("http://", 0) != 0) {
        return false;
    }
    std::wstring wUrl = utf8ToWide(validatedHttpUrl);
    HINSTANCE res = ShellExecuteW(nullptr, L"open", wUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(res) > 32;
}

static std::wstring quoteWin32Arg(const std::wstring& arg) {
    if (arg.empty()) return L"\"\"";
    bool needQuotes = arg.find_first_of(L" \t\n\v\"") != std::wstring::npos;
    if (!needQuotes) return arg;

    std::wstring out = L"\"";
    for (auto it = arg.begin();; ++it) {
        size_t backslashes = 0;
        while (it != arg.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }
        if (it == arg.end()) {
            out.append(backslashes * 2, L'\\');
            break;
        } else if (*it == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
        } else {
            out.append(backslashes, L'\\');
            out.push_back(*it);
        }
    }
    out.push_back(L'"');
    return out;
}

bool Win32PlatformServices::launchExecutable(const std::string& exePath,
                                             const std::vector<std::string>& args,
                                             const std::string& workingDir) {
    if (exePath.empty()) return false;
    std::wstring wExe = utf8ToWide(exePath);
    std::wstring cmdLine = quoteWin32Arg(wExe);
    for (const auto& a : args) {
        cmdLine.push_back(L' ');
        cmdLine += quoteWin32Arg(utf8ToWide(a));
    }

    std::wstring wDir = workingDir.empty() ? L"" : utf8ToWide(workingDir);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(
        wExe.c_str(),
        cmdLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_PROCESS_GROUP,
        nullptr,
        wDir.empty() ? nullptr : wDir.c_str(),
        &si,
        &pi);
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }
    return false;
}

bool Win32PlatformServices::openShellItem(const std::string& path) {
    if (path.empty()) return false;
    std::wstring wPath = utf8ToWide(path);
    HINSTANCE res = ShellExecuteW(nullptr, L"open", wPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(res) > 32;
}

core::KnownFolders Win32PlatformServices::getKnownFolders() const {
    core::KnownFolders kf;
    auto queryFolder = [](REFKNOWNFOLDERID id) -> std::string {
        PWSTR p = nullptr;
        std::string out;
        if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p)) && p) {
            out = wideToUtf8(p);
            CoTaskMemFree(p);
        }
        return out;
    };
    kf.desktop = queryFolder(FOLDERID_Desktop);
    kf.documents = queryFolder(FOLDERID_Documents);
    kf.downloads = queryFolder(FOLDERID_Downloads);
    kf.appData = getAppDataDirectory();
    kf.startMenuUser = queryFolder(FOLDERID_Programs);
    kf.startMenuCommon = queryFolder(FOLDERID_CommonPrograms);
    return kf;
}

core::Point Win32PlatformServices::getCursorPosition() const {
    POINT pt{};
    GetCursorPos(&pt);
    return {pt.x, pt.y};
}

core::MonitorWorkArea Win32PlatformServices::getMonitorWorkAreaAt(core::Point pt) const {
    POINT wpt{pt.x, pt.y};
    HMONITOR mon = MonitorFromPoint(wpt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    core::MonitorWorkArea out;
    if (GetMonitorInfoW(mon, &mi)) {
        out.workArea = {mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom};
    } else {
        out.workArea = {0, 0, 1280, 720};
    }
    out.dpiScale = static_cast<double>(GetDpiForSystem()) / 96.0;
    return out;
}

core::MonitorWorkArea Win32PlatformServices::getPrimaryMonitorWorkArea() const {
    return getMonitorWorkAreaAt({0, 0});
}

bool Win32PlatformServices::setStartupEnabled(bool enabled) {
    HKEY hKey = nullptr;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    bool ok = false;
    if (enabled) {
        wchar_t exePath[MAX_PATH]{};
        DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            std::wstring quoted = L"\"" + std::wstring(exePath, len) + L"\"";
            ok = (RegSetValueExW(
                      hKey, L"CmdPal", 0, REG_SZ,
                      reinterpret_cast<const BYTE*>(quoted.c_str()),
                      static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS);
        }
    } else {
        LONG r = RegDeleteValueW(hKey, L"CmdPal");
        ok = (r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND);
    }
    RegCloseKey(hKey);
    return ok;
}

bool Win32PlatformServices::isStartupEnabled() const {
    HKEY hKey = nullptr;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0;
    DWORD size = 0;
    LONG r = RegQueryValueExW(hKey, L"CmdPal", nullptr, &type, nullptr, &size);
    RegCloseKey(hKey);
    return r == ERROR_SUCCESS && type == REG_SZ && size > 0;
}

bool Win32PlatformServices::isDarkModePreferred() const {
    HKEY hKey = nullptr;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
        return true;
    }
    DWORD val = 1;
    DWORD size = sizeof(val);
    RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size);
    RegCloseKey(hKey);
    return val == 0;
}

int64_t Win32PlatformServices::currentTimeMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace cmdpal::platform

#endif  // _WIN32
