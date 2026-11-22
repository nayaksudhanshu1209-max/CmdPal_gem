#pragma once

#include "core/CoreServices.hpp"
#include "features/filesearch/FileSearchContract.hpp"
#include "features/quickfolders/QuickFoldersContract.hpp"
#include "features/settings/SettingsCoordinator.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

namespace cmdpal::features::settings {

class SettingsView {
public:
    SettingsView(core::CoreServices& core,
                 SettingsCoordinator& coordinator,
                 filesearch::IFileSearchService* fileSearch,
                 quickfolders::IQuickFoldersService* quickFolders);
    ~SettingsView();

    bool create(HINSTANCE hInst);
    void openSection(const std::string& section);
    HWND hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void buildControls();
    void loadValuesIntoControls();
    void saveControlsToSettings();
    void showSection(const std::string& section);

    core::CoreServices& core_;
    SettingsCoordinator& coordinator_;
    filesearch::IFileSearchService* fileSearch_;
    quickfolders::IQuickFoldersService* quickFolders_;

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    HWND navList_ = nullptr;

    // General controls
    HWND chkStartup_ = nullptr;
    HWND chkCtrl_ = nullptr;
    HWND chkAlt_ = nullptr;
    HWND chkShift_ = nullptr;
    HWND editKey_ = nullptr;
    HWND chkShowOnOpen_ = nullptr;
    HWND chkShowWhileTyping_ = nullptr;

    // Appearance controls
    HWND comboDensity_ = nullptr;
    HWND comboTheme_ = nullptr;
    HWND comboPlacement_ = nullptr;
    HWND chkAnimations_ = nullptr;

    // Hidden items
    HWND listHidden_ = nullptr;
    HWND btnRestoreHidden_ = nullptr;

    // File Search & Quick Folders
    HWND listRoots_ = nullptr;
    HWND editNewRoot_ = nullptr;
    HWND btnAddRoot_ = nullptr;
    HWND btnRemoveRoot_ = nullptr;
    HWND btnRebuildIndex_ = nullptr;
    HWND btnPauseResume_ = nullptr;

    // Web bangs & bookmarks
    HWND listBangs_ = nullptr;
    HWND editBangAlias_ = nullptr;
    HWND editBangLabel_ = nullptr;
    HWND editBangUrl_ = nullptr;
    HWND btnAddBang_ = nullptr;
    HWND btnRemoveBang_ = nullptr;

    // User commands
    HWND listCmds_ = nullptr;
    HWND editCmdName_ = nullptr;
    HWND editCmdExe_ = nullptr;
    HWND editCmdArgs_ = nullptr;
    HWND btnAddCmd_ = nullptr;
    HWND btnRemoveCmd_ = nullptr;

    // Notes
    HWND editNotesDir_ = nullptr;
    HWND editAutosaveMs_ = nullptr;

    // Footer
    HWND btnSave_ = nullptr;
    HWND btnReset_ = nullptr;

    std::string activeSection_ = "general";
};

}  // namespace cmdpal::features::settings

#endif  // _WIN32
