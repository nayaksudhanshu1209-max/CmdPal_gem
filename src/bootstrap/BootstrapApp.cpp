#include "bootstrap/Bootstrap.hpp"

#ifdef _WIN32
#include "features/notes/NotesView.hpp"
#include "features/palette/PaletteView.hpp"
#include "features/settings/SettingsView.hpp"
#include "platform/Win32PlatformServices.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#endif

namespace cmdpal::bootstrap {

int runApplication(int, char**) {
#ifdef _WIN32
    HANDLE singleInstanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\CmdPalSingleInstanceMutex");
    if (singleInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(singleInstanceMutex);
        return 0;
    }

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    std::string dataDir = platform::Win32PlatformServices::getAppDataDirectory();

    ApplicationContext app(std::make_unique<platform::Win32PlatformServices>(), dataDir);

    features::palette::PaletteView paletteView(app.core(), *app.registry().palette());
    features::notes::NotesView notesView(app.core(), *app.registry().notes());
    features::settings::SettingsView settingsView(
        app.core(),
        *app.registry().settings(),
        app.registry().fileSearch(),
        app.registry().quickFolders());

    if (!paletteView.create(hInst)) {
        if (singleInstanceMutex) CloseHandle(singleInstanceMutex);
        return 1;
    }
    notesView.create(hInst);
    settingsView.create(hInst);

    // Start deferred background indexing ONLY after palette window & hotkey are ready
    app.startDeferredBackgroundWork();

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    app.shutdown();
    if (singleInstanceMutex) CloseHandle(singleInstanceMutex);
    return static_cast<int>(msg.wParam);
#else
    return 0;
#endif
}

}  // namespace cmdpal::bootstrap
