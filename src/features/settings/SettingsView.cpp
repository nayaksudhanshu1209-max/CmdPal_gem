#include "features/settings/SettingsView.hpp"

#ifdef _WIN32

#include "platform/Win32PlatformServices.hpp"

#include <sstream>
#include <vector>

namespace cmdpal::features::settings {

namespace {

enum ControlId {
    ID_NAV_LIST = 2001,
    ID_BTN_SAVE = 2002,
    ID_BTN_RESET = 2003,
    ID_BTN_RESTORE_HIDDEN = 2004,
    ID_BTN_ADD_ROOT = 2005,
    ID_BTN_REMOVE_ROOT = 2006,
    ID_BTN_REBUILD_INDEX = 2007,
    ID_BTN_PAUSE_INDEX = 2008,
    ID_BTN_ADD_BANG = 2009,
    ID_BTN_REMOVE_BANG = 2010,
    ID_BTN_ADD_CMD = 2011,
    ID_BTN_REMOVE_CMD = 2012
};

std::wstring toW(const std::string& s) {
    return platform::Win32PlatformServices::utf8ToWide(s);
}
std::string fromW(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::wstring w(static_cast<size_t>(len), L'\0');
    if (len > 0) GetWindowTextW(h, w.data(), len + 1);
    return platform::Win32PlatformServices::wideToUtf8(w);
}

}  // namespace

SettingsView::SettingsView(core::CoreServices& core,
                           SettingsCoordinator& coordinator,
                           filesearch::IFileSearchService* fileSearch,
                           quickfolders::IQuickFoldersService* quickFolders)
    : core_(core),
      coordinator_(coordinator),
      fileSearch_(fileSearch),
      quickFolders_(quickFolders) {
    coordinator_.setOpenWindowCallback([this](const std::string& sec) { openSection(sec); });
}

SettingsView::~SettingsView() {
    if (hwnd_) DestroyWindow(hwnd_);
    if (font_) DeleteObject(font_);
}

bool SettingsView::create(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CmdPalSettingsWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"CmdPalSettingsWindow",
        L"CmdPal Settings",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 760, 560,
        nullptr, nullptr, hInst, this);
    if (!hwnd_) return false;

    font_ = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    buildControls();
    loadValuesIntoControls();
    showSection("general");
    return true;
}

void SettingsView::buildControls() {
    auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id = 0) {
        HWND c = CreateWindowExW(0, cls, text, WS_CHILD | style, x, y, w, h, hwnd_,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        return c;
    };

    navList_ = mk(L"LISTBOX", L"", WS_VISIBLE | WS_BORDER | LBS_NOTIFY, 12, 12, 175, 445, ID_NAV_LIST);
    SendMessageW(navList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"General"));
    SendMessageW(navList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Appearance"));
    SendMessageW(navList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Hidden Items"));
    SendMessageW(navList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"File Search & Folders"));
    SendMessageW(navList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Web Bangs"));
    SendMessageW(navList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Commands"));
    SendMessageW(navList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Notes"));
    SendMessageW(navList_, LB_SETCURSEL, 0, 0);

    // General
    chkStartup_ = mk(L"BUTTON", L"Run CmdPal at Windows startup (HKCU)", BS_AUTOCHECKBOX, 205, 24, 380, 26);
    chkCtrl_ = mk(L"BUTTON", L"Ctrl", BS_AUTOCHECKBOX, 205, 68, 65, 24);
    chkAlt_ = mk(L"BUTTON", L"Alt", BS_AUTOCHECKBOX, 275, 68, 60, 24);
    chkShift_ = mk(L"BUTTON", L"Shift", BS_AUTOCHECKBOX, 340, 68, 65, 24);
    editKey_ = mk(L"EDIT", L"Space", WS_BORDER | ES_AUTOHSCROLL, 415, 66, 90, 26);
    chkShowOnOpen_ = mk(L"BUTTON", L"Show suggestions when palette opens", BS_AUTOCHECKBOX, 205, 112, 380, 26);
    chkShowWhileTyping_ = mk(L"BUTTON", L"Show suggestions while typing", BS_AUTOCHECKBOX, 205, 146, 380, 26);

    // Appearance
    comboDensity_ = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST, 205, 24, 240, 120);
    SendMessageW(comboDensity_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Compact Density"));
    SendMessageW(comboDensity_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Comfortable Density"));

    comboTheme_ = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST, 205, 66, 240, 120);
    SendMessageW(comboTheme_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"System Theme"));
    SendMessageW(comboTheme_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Light Theme"));
    SendMessageW(comboTheme_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Dark Theme"));

    comboPlacement_ = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST, 205, 108, 240, 120);
    SendMessageW(comboPlacement_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Center of Screen"));
    SendMessageW(comboPlacement_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Top Center"));
    SendMessageW(comboPlacement_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Near Mouse Cursor"));

    chkAnimations_ = mk(L"BUTTON", L"Enable restrained open/close animations", BS_AUTOCHECKBOX, 205, 152, 380, 26);

    // Hidden items
    listHidden_ = mk(L"LISTBOX", L"", WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 205, 24, 515, 340);
    btnRestoreHidden_ = mk(L"BUTTON", L"Restore Selected Hidden Item", BS_PUSHBUTTON, 205, 376, 220, 30, ID_BTN_RESTORE_HIDDEN);

    // File Search & Folders
    listRoots_ = mk(L"LISTBOX", L"", WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 205, 24, 515, 220);
    editNewRoot_ = mk(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 205, 256, 360, 26);
    btnAddRoot_ = mk(L"BUTTON", L"Add Root", BS_PUSHBUTTON, 575, 254, 145, 28, ID_BTN_ADD_ROOT);
    btnRemoveRoot_ = mk(L"BUTTON", L"Remove Selected Root", BS_PUSHBUTTON, 205, 294, 175, 30, ID_BTN_REMOVE_ROOT);
    btnRebuildIndex_ = mk(L"BUTTON", L"Rebuild Index", BS_PUSHBUTTON, 390, 294, 150, 30, ID_BTN_REBUILD_INDEX);
    btnPauseResume_ = mk(L"BUTTON", L"Pause / Resume", BS_PUSHBUTTON, 550, 294, 170, 30, ID_BTN_PAUSE_INDEX);

    // Web Bangs
    listBangs_ = mk(L"LISTBOX", L"", WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 205, 24, 515, 200);
    editBangAlias_ = mk(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 205, 236, 90, 26);
    editBangLabel_ = mk(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 305, 236, 150, 26);
    editBangUrl_ = mk(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 465, 236, 255, 26);
    btnAddBang_ = mk(L"BUTTON", L"Add / Update Bang", BS_PUSHBUTTON, 205, 274, 160, 30, ID_BTN_ADD_BANG);
    btnRemoveBang_ = mk(L"BUTTON", L"Remove Selected", BS_PUSHBUTTON, 375, 274, 160, 30, ID_BTN_REMOVE_BANG);

    // Commands
    listCmds_ = mk(L"LISTBOX", L"", WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 205, 24, 515, 200);
    editCmdName_ = mk(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 205, 236, 150, 26);
    editCmdExe_ = mk(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 365, 236, 200, 26);
    editCmdArgs_ = mk(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 575, 236, 145, 26);
    btnAddCmd_ = mk(L"BUTTON", L"Add Command", BS_PUSHBUTTON, 205, 274, 150, 30, ID_BTN_ADD_CMD);
    btnRemoveCmd_ = mk(L"BUTTON", L"Remove Selected", BS_PUSHBUTTON, 365, 274, 150, 30, ID_BTN_REMOVE_CMD);

    // Notes
    editNotesDir_ = mk(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 205, 24, 515, 26);
    editAutosaveMs_ = mk(L"EDIT", L"600", WS_BORDER | ES_AUTOHSCROLL, 205, 64, 120, 26);

    // Footer
    btnSave_ = mk(L"BUTTON", L"Save & Apply", WS_VISIBLE | BS_DEFPUSHBUTTON, 575, 472, 145, 32, ID_BTN_SAVE);
    btnReset_ = mk(L"BUTTON", L"Reset Defaults", WS_VISIBLE | BS_PUSHBUTTON, 420, 472, 145, 32, ID_BTN_RESET);
}

void SettingsView::showSection(const std::string& section) {
    activeSection_ = section;
    std::vector<HWND> all = {
        chkStartup_, chkCtrl_, chkAlt_, chkShift_, editKey_, chkShowOnOpen_, chkShowWhileTyping_,
        comboDensity_, comboTheme_, comboPlacement_, chkAnimations_,
        listHidden_, btnRestoreHidden_,
        listRoots_, editNewRoot_, btnAddRoot_, btnRemoveRoot_, btnRebuildIndex_, btnPauseResume_,
        listBangs_, editBangAlias_, editBangLabel_, editBangUrl_, btnAddBang_, btnRemoveBang_,
        listCmds_, editCmdName_, editCmdExe_, editCmdArgs_, btnAddCmd_, btnRemoveCmd_,
        editNotesDir_, editAutosaveMs_
    };
    for (HWND h : all) ShowWindow(h, SW_HIDE);

    auto show = [](std::initializer_list<HWND> list) {
        for (HWND h : list) ShowWindow(h, SW_SHOW);
    };

    if (section == "general") {
        show({chkStartup_, chkCtrl_, chkAlt_, chkShift_, editKey_, chkShowOnOpen_, chkShowWhileTyping_});
    } else if (section == "appearance") {
        show({comboDensity_, comboTheme_, comboPlacement_, chkAnimations_});
    } else if (section == "categories") {
        show({listHidden_, btnRestoreHidden_});
    } else if (section == "files") {
        show({listRoots_, editNewRoot_, btnAddRoot_, btnRemoveRoot_, btnRebuildIndex_, btnPauseResume_});
    } else if (section == "web") {
        show({listBangs_, editBangAlias_, editBangLabel_, editBangUrl_, btnAddBang_, btnRemoveBang_});
    } else if (section == "commands") {
        show({listCmds_, editCmdName_, editCmdExe_, editCmdArgs_, btnAddCmd_, btnRemoveCmd_});
    } else if (section == "notes") {
        show({editNotesDir_, editAutosaveMs_});
    }
}

void SettingsView::loadValuesIntoControls() {
    auto s = core_.settings().snapshot();
    SendMessageW(chkStartup_, BM_SETCHECK, s.runAtStartup ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(chkCtrl_, BM_SETCHECK, s.globalShortcut.ctrl ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(chkAlt_, BM_SETCHECK, s.globalShortcut.alt ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(chkShift_, BM_SETCHECK, s.globalShortcut.shift ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(editKey_, toW(s.globalShortcut.key).c_str());
    SendMessageW(chkShowOnOpen_, BM_SETCHECK, s.showSuggestionsOnOpen ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(chkShowWhileTyping_, BM_SETCHECK, s.showSuggestionsWhileTyping ? BST_CHECKED : BST_UNCHECKED, 0);

    SendMessageW(comboDensity_, CB_SETCURSEL, s.density == core::DensityMode::Comfortable ? 1 : 0, 0);
    SendMessageW(comboTheme_, CB_SETCURSEL,
                 s.theme == core::ThemeMode::Light ? 1 : (s.theme == core::ThemeMode::Dark ? 2 : 0), 0);
    SendMessageW(comboPlacement_, CB_SETCURSEL,
                 s.placement == core::PalettePlacement::TopCenter ? 1
                 : (s.placement == core::PalettePlacement::NearCursor ? 2 : 0), 0);
    SendMessageW(chkAnimations_, BM_SETCHECK, s.animationsEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

    SendMessageW(listHidden_, LB_RESETCONTENT, 0, 0);
    for (const auto& id : core_.usage().hiddenApps()) {
        std::string line = "[App] " + id;
        SendMessageW(listHidden_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(toW(line).c_str()));
    }
    for (const auto& id : core_.usage().hiddenResults()) {
        std::string line = "[Result] " + id;
        SendMessageW(listHidden_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(toW(line).c_str()));
    }

    SendMessageW(listRoots_, LB_RESETCONTENT, 0, 0);
    for (const auto& r : s.fileRoots) {
        std::string line = (r.enabled ? "[ON] " : "[OFF] ") + r.label + " — " + r.path;
        SendMessageW(listRoots_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(toW(line).c_str()));
    }

    SendMessageW(listBangs_, LB_RESETCONTENT, 0, 0);
    for (const auto& b : s.webBangs) {
        std::string line = b.alias + " -> " + b.label + " (" + b.urlTemplate + ")";
        SendMessageW(listBangs_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(toW(line).c_str()));
    }

    SendMessageW(listCmds_, LB_RESETCONTENT, 0, 0);
    for (const auto& c : s.userCommands) {
        std::string line = c.name + " -> " + c.executable;
        SendMessageW(listCmds_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(toW(line).c_str()));
    }

    SetWindowTextW(editNotesDir_, toW(s.notesDirectory).c_str());
    SetWindowTextW(editAutosaveMs_, toW(std::to_string(s.noteAutosaveDebounceMs)).c_str());
}

void SettingsView::saveControlsToSettings() {
    auto s = core_.settings().snapshot();
    s.runAtStartup = (SendMessageW(chkStartup_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.globalShortcut.ctrl = (SendMessageW(chkCtrl_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.globalShortcut.alt = (SendMessageW(chkAlt_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.globalShortcut.shift = (SendMessageW(chkShift_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.globalShortcut.key = fromW(editKey_);
    s.showSuggestionsOnOpen = (SendMessageW(chkShowOnOpen_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.showSuggestionsWhileTyping = (SendMessageW(chkShowWhileTyping_, BM_GETCHECK, 0, 0) == BST_CHECKED);

    int dens = static_cast<int>(SendMessageW(comboDensity_, CB_GETCURSEL, 0, 0));
    s.density = (dens == 1) ? core::DensityMode::Comfortable : core::DensityMode::Compact;

    int thm = static_cast<int>(SendMessageW(comboTheme_, CB_GETCURSEL, 0, 0));
    s.theme = (thm == 1) ? core::ThemeMode::Light : (thm == 2 ? core::ThemeMode::Dark : core::ThemeMode::System);

    int plc = static_cast<int>(SendMessageW(comboPlacement_, CB_GETCURSEL, 0, 0));
    s.placement = (plc == 1) ? core::PalettePlacement::TopCenter
                  : (plc == 2 ? core::PalettePlacement::NearCursor : core::PalettePlacement::Center);

    s.animationsEnabled = (SendMessageW(chkAnimations_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    std::string nd = fromW(editNotesDir_);
    if (!nd.empty()) s.notesDirectory = nd;
    try {
        s.noteAutosaveDebounceMs = std::stoi(fromW(editAutosaveMs_));
    } catch (...) {}

    coordinator_.applySettings(s);
}

void SettingsView::openSection(const std::string& section) {
    if (!hwnd_) return;
    loadValuesIntoControls();
    showSection(section);
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
}

LRESULT CALLBACK SettingsView::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<SettingsView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<SettingsView*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            if (id == ID_NAV_LIST && code == LBN_SELCHANGE) {
                int sel = static_cast<int>(SendMessageW(self->navList_, LB_GETCURSEL, 0, 0));
                static const char* sections[] = {"general", "appearance", "categories", "files", "web", "commands", "notes"};
                if (sel >= 0 && sel < 7) self->showSection(sections[sel]);
            } else if (id == ID_BTN_SAVE) {
                self->saveControlsToSettings();
                ShowWindow(hwnd, SW_HIDE);
            } else if (id == ID_BTN_RESET) {
                self->core_.settings().resetToDefaults();
                self->core_.events().publish(core::SettingsChangedEvent{});
                self->loadValuesIntoControls();
            } else if (id == ID_BTN_RESTORE_HIDDEN) {
                int sel = static_cast<int>(SendMessageW(self->listHidden_, LB_GETCURSEL, 0, 0));
                auto apps = self->core_.usage().hiddenApps();
                auto res = self->core_.usage().hiddenResults();
                if (sel >= 0 && sel < static_cast<int>(apps.size())) {
                    self->core_.usage().restoreApp(apps[static_cast<size_t>(sel)]);
                } else if (sel >= static_cast<int>(apps.size()) &&
                           sel < static_cast<int>(apps.size() + res.size())) {
                    self->core_.usage().restoreResult(res[static_cast<size_t>(sel) - apps.size()]);
                }
                self->loadValuesIntoControls();
            } else if (id == ID_BTN_ADD_ROOT && self->fileSearch_) {
                std::string path = fromW(self->editNewRoot_);
                if (!path.empty()) {
                    self->fileSearch_->addRoot(path, path);
                    SetWindowTextW(self->editNewRoot_, L"");
                    self->loadValuesIntoControls();
                }
            } else if (id == ID_BTN_REMOVE_ROOT && self->fileSearch_) {
                int sel = static_cast<int>(SendMessageW(self->listRoots_, LB_GETCURSEL, 0, 0));
                auto roots = self->core_.settings().snapshot().fileRoots;
                if (sel >= 0 && sel < static_cast<int>(roots.size())) {
                    self->fileSearch_->removeRoot(roots[static_cast<size_t>(sel)].id);
                    self->loadValuesIntoControls();
                }
            } else if (id == ID_BTN_REBUILD_INDEX && self->fileSearch_) {
                self->fileSearch_->rebuildIndex();
            } else if (id == ID_BTN_PAUSE_INDEX && self->fileSearch_) {
                if (self->fileSearch_->status().paused) self->fileSearch_->resumeIndexing();
                else self->fileSearch_->pauseIndexing();
            } else if (id == ID_BTN_ADD_BANG) {
                auto s = self->core_.settings().snapshot();
                std::string alias = fromW(self->editBangAlias_);
                std::string label = fromW(self->editBangLabel_);
                std::string url = fromW(self->editBangUrl_);
                if (!alias.empty() && !url.empty()) {
                    s.webBangs.push_back({"bang-" + alias, alias, label.empty() ? alias : label, url, true});
                    self->coordinator_.applySettings(s);
                    self->loadValuesIntoControls();
                }
            } else if (id == ID_BTN_REMOVE_BANG) {
                int sel = static_cast<int>(SendMessageW(self->listBangs_, LB_GETCURSEL, 0, 0));
                auto s = self->core_.settings().snapshot();
                if (sel >= 0 && sel < static_cast<int>(s.webBangs.size())) {
                    s.webBangs.erase(s.webBangs.begin() + sel);
                    self->coordinator_.applySettings(s);
                    self->loadValuesIntoControls();
                }
            } else if (id == ID_BTN_ADD_CMD) {
                auto s = self->core_.settings().snapshot();
                std::string name = fromW(self->editCmdName_);
                std::string exe = fromW(self->editCmdExe_);
                std::string argStr = fromW(self->editCmdArgs_);
                if (!name.empty() && !exe.empty()) {
                    core::UserCommandDef cmd;
                    cmd.id = "uc-" + name;
                    cmd.name = name;
                    cmd.executable = exe;
                    std::istringstream iss(argStr);
                    std::string tok;
                    while (iss >> tok) cmd.args.push_back(tok);
                    s.userCommands.push_back(std::move(cmd));
                    self->coordinator_.applySettings(s);
                    self->loadValuesIntoControls();
                }
            } else if (id == ID_BTN_REMOVE_CMD) {
                int sel = static_cast<int>(SendMessageW(self->listCmds_, LB_GETCURSEL, 0, 0));
                auto s = self->core_.settings().snapshot();
                if (sel >= 0 && sel < static_cast<int>(s.userCommands.size())) {
                    s.userCommands.erase(s.userCommands.begin() + sel);
                    self->coordinator_.applySettings(s);
                    self->loadValuesIntoControls();
                }
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace cmdpal::features::settings

#endif  // _WIN32
