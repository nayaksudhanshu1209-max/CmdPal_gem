#include "features/palette/PaletteView.hpp"

#ifdef _WIN32

#include "core/SearchMatch.hpp"
#include "platform/Win32PlatformServices.hpp"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

namespace cmdpal::features::palette {

namespace {

constexpr int kHotkeyId = 1;
constexpr UINT kTrayCallbackMsg = WM_APP + 10;
constexpr UINT kTrayMenuOpen = 1001;
constexpr UINT kTrayMenuNotes = 1002;
constexpr UINT kTrayMenuSettings = 1003;
constexpr UINT kTrayMenuExit = 1004;
constexpr UINT kCtxHideResult = 1010;

UINT keyNameToVk(const std::string& key) {
    std::string k = core::toLowerAscii(key);
    if (k == "space" || k == " ") return VK_SPACE;
    if (k.size() == 1 && std::isalnum(static_cast<unsigned char>(k[0]))) {
        return static_cast<UINT>(std::toupper(static_cast<unsigned char>(k[0])));
    }
    if (k == "f1") return VK_F1;
    if (k == "f2") return VK_F2;
    if (k == "f3") return VK_F3;
    if (k == "f4") return VK_F4;
    return VK_SPACE;
}

}  // namespace

PaletteView::PaletteView(core::CoreServices& core, PaletteCoordinator& coordinator)
    : core_(core), coordinator_(coordinator) {
    core_.events().subscribe<core::SettingsChangedEvent>([this](const core::SettingsChangedEvent&) {
        applyHotkey();
        if (hwnd_ && IsWindowVisible(hwnd_)) {
            updateLayout();
            InvalidateRect(hwnd_, nullptr, TRUE);
        }
    });
}

PaletteView::~PaletteView() {
    if (hwnd_) {
        UnregisterHotKey(hwnd_, kHotkeyId);
        if (trayAdded_) {
            NOTIFYICONDATAW nid{};
            nid.cbSize = sizeof(nid);
            nid.hWnd = hwnd_;
            nid.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &nid);
        }
        DestroyWindow(hwnd_);
    }
    if (fontTitle_) DeleteObject(fontTitle_);
    if (fontSub_) DeleteObject(fontSub_);
    if (fontInput_) DeleteObject(fontInput_);
    if (bgBrush_) DeleteObject(bgBrush_);
}

bool PaletteView::isDark() const {
    auto theme = core_.settings().snapshot().theme;
    if (theme == core::ThemeMode::Dark) return true;
    if (theme == core::ThemeMode::Light) return false;
    return core_.platform().isDarkModePreferred();
}

bool PaletteView::create(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CmdPalPaletteWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"CmdPalPaletteWindow",
        L"CmdPal",
        WS_POPUP,
        0, 0, 680, 72,
        nullptr, nullptr, hInst, this);
    if (!hwnd_) return false;

    SetLayeredWindowAttributes(hwnd_, 0, 250, LWA_ALPHA);
    int cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    fontInput_ = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    fontTitle_ = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    fontSub_ = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    editHwnd_ = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        18, 16, 644, 34,
        hwnd_, reinterpret_cast<HMENU>(101), hInst, nullptr);
    SendMessageW(editHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(fontInput_), TRUE);
    SetWindowSubclass(editHwnd_, EditSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));

    // Tray icon
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = kTrayCallbackMsg;
    nid.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    wcscpy_s(nid.szTip, L"CmdPal");
    trayAdded_ = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;

    applyHotkey();
    return true;
}

void PaletteView::applyHotkey() {
    if (!hwnd_) return;
    UnregisterHotKey(hwnd_, kHotkeyId);
    auto sc = core_.settings().snapshot().globalShortcut;
    UINT mods = MOD_NOREPEAT;
    if (sc.ctrl) mods |= MOD_CONTROL;
    if (sc.alt) mods |= MOD_ALT;
    if (sc.shift) mods |= MOD_SHIFT;
    if (sc.win) mods |= MOD_WIN;
    RegisterHotKey(hwnd_, kHotkeyId, mods, keyNameToVk(sc.key));
}

void PaletteView::updateLayout() {
    if (!hwnd_) return;
    auto s = core_.settings().snapshot();
    int rowHeight = (s.density == core::DensityMode::Compact) ? 46 : 58;
    int count = static_cast<int>(coordinator_.state().results.size());
    int inputHeight = 62;
    int listHeight = count > 0 ? count * rowHeight + 12 : (coordinator_.state().statusMessage.empty() ? 0 : 42);
    int totalWidth = 680;
    int totalHeight = inputHeight + listHeight;

    core::Point cursor = core_.platform().getCursorPosition();
    core::MonitorWorkArea mon = (s.placement == core::PalettePlacement::NearCursor)
                                    ? core_.platform().getMonitorWorkAreaAt(cursor)
                                    : core_.platform().getPrimaryMonitorWorkArea();

    core::Rect bounds = PaletteCoordinator::computeBounds(
        s.placement, mon, cursor, totalWidth, totalHeight);

    SetWindowPos(hwnd_, HWND_TOPMOST,
                 bounds.left, bounds.top, bounds.width(), bounds.height(),
                 SWP_NOACTIVATE);

    int chipWidth = 0;
    if (coordinator_.state().activeChip.has_value()) {
        chipWidth = 132;
    }
    MoveWindow(editHwnd_, 18 + chipWidth, 16, bounds.width() - 36 - chipWidth, 34, TRUE);
}

void PaletteView::show() {
    if (!hwnd_) return;
    coordinator_.openPalette();
    suppressEditChange_ = true;
    SetWindowTextW(editHwnd_, L"");
    suppressEditChange_ = false;
    hoverIndex_ = -1;
    updateLayout();

    auto s = core_.settings().snapshot();
    if (s.animationsEnabled && s.animationDurationMs > 0) {
        AnimateWindow(hwnd_, static_cast<DWORD>(std::min(120, s.animationDurationMs)), AW_BLEND);
    } else {
        ShowWindow(hwnd_, SW_SHOW);
    }
    SetForegroundWindow(hwnd_);
    SetFocus(editHwnd_);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void PaletteView::hide() {
    if (!hwnd_) return;
    coordinator_.closePalette();
    ShowWindow(hwnd_, SW_HIDE);
}

void PaletteView::toggle() {
    if (hwnd_ && IsWindowVisible(hwnd_)) {
        hide();
    } else {
        show();
    }
}

void PaletteView::onQueryChanged() {
    if (suppressEditChange_) return;
    int len = GetWindowTextLengthW(editHwnd_);
    std::wstring wText(static_cast<size_t>(len), L'\0');
    if (len > 0) {
        GetWindowTextW(editHwnd_, wText.data(), len + 1);
    }
    std::string utf8 = platform::Win32PlatformServices::wideToUtf8(wText);
    coordinator_.updateQuery(utf8, false);
    hoverIndex_ = -1;
    updateLayout();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void PaletteView::onCommandExecute(int index) {
    const auto& results = coordinator_.state().results;
    if (index < 0 || index >= static_cast<int>(results.size())) return;
    const auto& item = results[static_cast<size_t>(index)];
    if (item.requiresConfirmation) {
        std::wstring msg = platform::Win32PlatformServices::utf8ToWide(
            item.confirmationMessage.empty() ? ("Execute " + item.title + "?") : item.confirmationMessage);
        if (MessageBoxW(hwnd_, msg.c_str(), L"CmdPal Confirmation", MB_YESNO | MB_ICONWARNING | MB_TOPMOST) != IDYES) {
            return;
        }
    }
    auto res = coordinator_.activateIndex(index);
    if (res.hidePalette) {
        hide();
    } else {
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

void PaletteView::onPaint(HDC hdc) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    bool dark = isDark();

    COLORREF bg = dark ? RGB(24, 26, 32) : RGB(248, 250, 252);
    COLORREF border = dark ? RGB(60, 68, 84) : RGB(203, 213, 225);
    COLORREF inputBg = dark ? RGB(34, 38, 48) : RGB(255, 255, 255);
    COLORREF selBg = dark ? RGB(45, 78, 122) : RGB(219, 234, 254);
    COLORREF hovBg = dark ? RGB(36, 42, 54) : RGB(241, 245, 249);
    COLORREF txtMain = dark ? RGB(241, 245, 249) : RGB(15, 23, 42);
    COLORREF txtSub = dark ? RGB(148, 163, 184) : RGB(100, 116, 139);
    COLORREF chipBg = dark ? RGB(14, 116, 144) : RGB(14, 165, 233);

    HBRUSH hBg = CreateSolidBrush(bg);
    FillRect(hdc, &rc, hBg);
    DeleteObject(hBg);

    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 0, 0, rc.right, rc.bottom);

    RECT inputRc{10, 10, rc.right - 10, 54};
    HBRUSH hIn = CreateSolidBrush(inputBg);
    FillRect(hdc, &inputRc, hIn);
    DeleteObject(hIn);

    SetBkMode(hdc, TRANSPARENT);
    if (coordinator_.state().activeChip.has_value()) {
        RECT chipRc{16, 16, 140, 48};
        HBRUSH hChip = CreateSolidBrush(chipBg);
        FillRect(hdc, &chipRc, hChip);
        DeleteObject(hChip);
        SelectObject(hdc, fontSub_);
        SetTextColor(hdc, RGB(255, 255, 255));
        std::wstring label = platform::Win32PlatformServices::utf8ToWide(coordinator_.state().activeChip->label);
        DrawTextW(hdc, label.c_str(), -1, &chipRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    auto s = core_.settings().snapshot();
    int rowHeight = (s.density == core::DensityMode::Compact) ? 46 : 58;
    const auto& results = coordinator_.state().results;

    if (results.empty() && !coordinator_.state().statusMessage.empty()) {
        RECT stRc{18, 64, rc.right - 18, 96};
        SelectObject(hdc, fontSub_);
        SetTextColor(hdc, txtSub);
        std::wstring msg = platform::Win32PlatformServices::utf8ToWide(coordinator_.state().statusMessage);
        DrawTextW(hdc, msg.c_str(), -1, &stRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    int y = 62;
    for (int i = 0; i < static_cast<int>(results.size()); ++i) {
        const auto& item = results[static_cast<size_t>(i)];
        RECT rowRc{10, y, rc.right - 10, y + rowHeight - 2};
        if (i == coordinator_.state().selectedIndex) {
            HBRUSH hSel = CreateSolidBrush(selBg);
            FillRect(hdc, &rowRc, hSel);
            DeleteObject(hSel);
        } else if (i == hoverIndex_) {
            HBRUSH hHov = CreateSolidBrush(hovBg);
            FillRect(hdc, &rowRc, hHov);
            DeleteObject(hHov);
        }

        // Category badge on right (NO Alt+number hints!)
        RECT catRc{rowRc.right - 120, y + 6, rowRc.right - 10, y + 26};
        SelectObject(hdc, fontSub_);
        SetTextColor(hdc, txtSub);
        std::wstring catW = platform::Win32PlatformServices::utf8ToWide(item.categoryLabel);
        DrawTextW(hdc, catW.c_str(), -1, &catRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        RECT titleRc{rowRc.left + 12, y + 4, rowRc.right - 130, y + 26};
        SelectObject(hdc, fontTitle_);
        SetTextColor(hdc, txtMain);
        std::wstring titleW = platform::Win32PlatformServices::utf8ToWide(item.title);
        DrawTextW(hdc, titleW.c_str(), -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT subRc{rowRc.left + 12, y + 24, rowRc.right - 16, y + rowHeight - 4};
        SelectObject(hdc, fontSub_);
        SetTextColor(hdc, txtSub);
        std::wstring subW = platform::Win32PlatformServices::utf8ToWide(item.subtitle);
        DrawTextW(hdc, subW.c_str(), -1, &subRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        y += rowHeight;
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}

LRESULT CALLBACK PaletteView::EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                               UINT_PTR, DWORD_PTR dwRefData) {
    auto* self = reinterpret_cast<PaletteView*>(dwRefData);
    if (self && msg == WM_KEYDOWN) {
        switch (wParam) {
            case VK_UP:
                self->coordinator_.handleKey(NavKey::Up);
                InvalidateRect(self->hwnd_, nullptr, FALSE);
                return 0;
            case VK_DOWN:
                self->coordinator_.handleKey(NavKey::Down);
                InvalidateRect(self->hwnd_, nullptr, FALSE);
                return 0;
            case VK_TAB:
                self->coordinator_.handleKey((GetKeyState(VK_SHIFT) & 0x8000) ? NavKey::ShiftTab : NavKey::Tab);
                InvalidateRect(self->hwnd_, nullptr, FALSE);
                return 0;
            case VK_PRIOR:
                self->coordinator_.handleKey(NavKey::PageUp);
                InvalidateRect(self->hwnd_, nullptr, FALSE);
                return 0;
            case VK_NEXT:
                self->coordinator_.handleKey(NavKey::PageDown);
                InvalidateRect(self->hwnd_, nullptr, FALSE);
                return 0;
            case VK_RETURN:
                self->onCommandExecute(self->coordinator_.state().selectedIndex);
                return 0;
            case VK_ESCAPE:
                self->hide();
                return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK PaletteView::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<PaletteView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<PaletteView*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_HOTKEY:
            if (wParam == kHotkeyId) self->toggle();
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE && IsWindowVisible(hwnd)) {
                self->hide();
            }
            return 0;
        case WM_COMMAND:
            if (HIWORD(wParam) == EN_CHANGE && reinterpret_cast<HWND>(lParam) == self->editHwnd_) {
                self->onQueryChanged();
            }
            return 0;
        case WM_CTLCOLOREDIT: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            bool dark = self->isDark();
            COLORREF bg = dark ? RGB(34, 38, 48) : RGB(255, 255, 255);
            SetBkColor(hdc, bg);
            SetTextColor(hdc, dark ? RGB(241, 245, 249) : RGB(15, 23, 42));
            if (self->bgBrush_) DeleteObject(self->bgBrush_);
            self->bgBrush_ = CreateSolidBrush(bg);
            return reinterpret_cast<LRESULT>(self->bgBrush_);
        }
        case WM_MOUSEMOVE: {
            int y = HIWORD(lParam);
            int rowH = (self->core_.settings().snapshot().density == core::DensityMode::Compact) ? 46 : 58;
            int idx = (y >= 62) ? (y - 62) / rowH : -1;
            if (idx >= static_cast<int>(self->coordinator_.state().results.size())) idx = -1;
            if (idx != self->hoverIndex_) {
                self->hoverIndex_ = idx;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int y = HIWORD(lParam);
            int rowH = (self->core_.settings().snapshot().density == core::DensityMode::Compact) ? 46 : 58;
            int idx = (y >= 62) ? (y - 62) / rowH : -1;
            if (idx >= 0 && idx < static_cast<int>(self->coordinator_.state().results.size())) {
                self->onCommandExecute(idx);
            }
            return 0;
        }
        case WM_RBUTTONDOWN: {
            int y = HIWORD(lParam);
            int rowH = (self->core_.settings().snapshot().density == core::DensityMode::Compact) ? 46 : 58;
            int idx = (y >= 62) ? (y - 62) / rowH : -1;
            const auto& results = self->coordinator_.state().results;
            if (idx >= 0 && idx < static_cast<int>(results.size()) && results[static_cast<size_t>(idx)].canHide) {
                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING, kCtxHideResult,
                            results[static_cast<size_t>(idx)].isApp ? L"Hide Application" : L"Hide Result");
                POINT pt{};
                GetCursorPos(&pt);
                UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
                if (cmd == kCtxHideResult) {
                    self->coordinator_.hideResultAt(idx);
                    self->updateLayout();
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            }
            return 0;
        }
        case kTrayCallbackMsg:
            if (lParam == WM_LBUTTONUP) {
                self->toggle();
            } else if (lParam == WM_RBUTTONUP) {
                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING, kTrayMenuOpen, L"Open CmdPal");
                AppendMenuW(menu, MF_STRING, kTrayMenuNotes, L"Open Notes");
                AppendMenuW(menu, MF_STRING, kTrayMenuSettings, L"Settings");
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, kTrayMenuExit, L"Exit CmdPal");
                POINT pt{};
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);
                UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
                if (cmd == kTrayMenuOpen) self->show();
                else if (cmd == kTrayMenuNotes) self->core_.events().publish(core::OpenNoteEvent{""});
                else if (cmd == kTrayMenuSettings) self->core_.events().publish(core::OpenSettingsEvent{"general"});
                else if (cmd == kTrayMenuExit) PostQuitMessage(0);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            self->onPaint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace cmdpal::features::palette

#endif  // _WIN32
