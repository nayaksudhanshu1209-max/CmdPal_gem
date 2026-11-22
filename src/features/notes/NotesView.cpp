#include "features/notes/NotesView.hpp"

#ifdef _WIN32

#include "platform/Win32PlatformServices.hpp"

#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

namespace cmdpal::features::notes {

namespace {

constexpr UINT_PTR kAutosaveTimerId = 42;
constexpr int kToolbarHeight = 40;
constexpr int kPadding = 18;
constexpr int kLineHeight = 22;
constexpr int kResizeBorder = 8;

}  // namespace

NotesView::NotesView(core::CoreServices& core, NotesCoordinator& coordinator)
    : core_(core), coordinator_(coordinator) {
    core_.events().subscribe<core::OpenNoteEvent>([this](const core::OpenNoteEvent& ev) {
        openNote(ev.noteId);
    });
}

NotesView::~NotesView() {
    if (hwnd_) {
        KillTimer(hwnd_, kAutosaveTimerId);
        DestroyWindow(hwnd_);
    }
    if (fontNormal_) DeleteObject(fontNormal_);
    if (fontBold_) DeleteObject(fontBold_);
    if (fontItalic_) DeleteObject(fontItalic_);
    if (fontMono_) DeleteObject(fontMono_);
    if (fontH1_) DeleteObject(fontH1_);
    if (fontH2_) DeleteObject(fontH2_);
    if (fontSmall_) DeleteObject(fontSmall_);
}

bool NotesView::create(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"CmdPalNotesWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    RegisterClassExW(&wc);

    // Frameless, resizable (WS_POPUP | WS_THICKFRAME), NOT always-on-top (WS_EX_APPWINDOW only)
    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"CmdPalNotesWindow",
        L"CmdPal Notes",
        WS_POPUP | WS_THICKFRAME,
        180, 140, 620, 480,
        nullptr, nullptr, hInst, this);
    if (!hwnd_) return false;

    int cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    fontNormal_ = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    fontBold_ = CreateFontW(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    fontItalic_ = CreateFontW(-15, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    fontMono_ = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    fontH1_ = CreateFontW(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    fontH2_ = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    fontSmall_ = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    return true;
}

void NotesView::refreshToolbar() {
    toolbarActions_ = coordinator_.toolbarActionsForNote(activeNoteId_);
}

void NotesView::openNote(const std::string& noteId) {
    if (!hwnd_) return;

    std::string targetId = noteId;
    if (targetId == "__new__" || targetId.empty()) {
        auto existing = coordinator_.listNotes();
        if (targetId.empty() && !existing.empty()) {
            targetId = existing.front().id;
        } else {
            auto created = coordinator_.createNote("Quick Note");
            if (created) targetId = created->id;
        }
    }

    activeNoteId_ = targetId;
    content_ = coordinator_.loadNoteContent(activeNoteId_);
    caretPos_ = content_.size();
    selAnchor_ = caretPos_;
    dirty_ = false;
    scrollY_ = 0;
    toolbarScrollX_ = 0;
    undoStack_.reset(content_, caretPos_);
    refreshToolbar();

    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

bool NotesView::saveNow() {
    if (activeNoteId_.empty()) return true;
    bool ok = coordinator_.saveNoteContent(activeNoteId_, content_);
    if (ok) dirty_ = false;
    return ok;
}

void NotesView::closeAndSave() {
    KillTimer(hwnd_, kAutosaveTimerId);
    if (!saveNow()) {
        int choice = MessageBoxW(
            hwnd_,
            L"Could not save note changes to disk. Close anyway and discard unsaved edits?",
            L"CmdPal Notes - Save Failed",
            MB_YESNO | MB_ICONWARNING);
        if (choice != IDYES) return;
    }
    ShowWindow(hwnd_, SW_HIDE);
}

void NotesView::triggerAutosaveTimer() {
    dirty_ = true;
    int delay = core_.settings().snapshot().noteAutosaveDebounceMs;
    SetTimer(hwnd_, kAutosaveTimerId, static_cast<UINT>(delay), nullptr);
}

void NotesView::applyAction(const ToolbarAction& action) {
    auto res = NotesMarkdown::applyToolbarAction(content_, selAnchor_, caretPos_, action);
    content_ = std::move(res.newContent);
    selAnchor_ = res.newCaretStart;
    caretPos_ = res.newCaretEnd;
    undoStack_.push(content_, caretPos_);
    coordinator_.recordToolbarUsage(activeNoteId_, action.id);
    refreshToolbar();
    triggerAutosaveTimer();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void NotesView::onChar(wchar_t ch) {
    if (ch < 0x20 && ch != L'\r' && ch != L'\t') return;
    if (ch == L'\r') {
        auto res = NotesMarkdown::handleEnterKey(content_, caretPos_);
        content_ = std::move(res.newContent);
        caretPos_ = res.newCaretStart;
        selAnchor_ = caretPos_;
    } else {
        std::string ins = (ch == L'\t') ? "    " : platform::Win32PlatformServices::wideToUtf8(std::wstring(1, ch));
        size_t a = std::min(selAnchor_, caretPos_);
        size_t b = std::max(selAnchor_, caretPos_);
        content_.replace(a, b - a, ins);
        caretPos_ = a + ins.size();
        selAnchor_ = caretPos_;
    }
    undoStack_.push(content_, caretPos_);
    triggerAutosaveTimer();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void NotesView::onKeyDown(WPARAM vk) {
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (vk == VK_ESCAPE) {
        closeAndSave();
        return;
    }
    if (ctrl && vk == 'S') {
        saveNow();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    if (ctrl && vk == 'Z') {
        if (auto snap = undoStack_.undo()) {
            content_ = snap->content;
            caretPos_ = std::min(snap->caretPos, content_.size());
            selAnchor_ = caretPos_;
            triggerAutosaveTimer();
            InvalidateRect(hwnd_, nullptr, TRUE);
        }
        return;
    }
    if (ctrl && vk == 'Y') {
        if (auto snap = undoStack_.redo()) {
            content_ = snap->content;
            caretPos_ = std::min(snap->caretPos, content_.size());
            selAnchor_ = caretPos_;
            triggerAutosaveTimer();
            InvalidateRect(hwnd_, nullptr, TRUE);
        }
        return;
    }
    if (ctrl && vk == 'B') {
        for (const auto& a : toolbarActions_) if (a.id == "bold") { applyAction(a); break; }
        return;
    }
    if (ctrl && vk == 'I') {
        for (const auto& a : toolbarActions_) if (a.id == "italic") { applyAction(a); break; }
        return;
    }
    if (ctrl && vk == 'K') {
        for (const auto& a : toolbarActions_) if (a.id == "link") { applyAction(a); break; }
        return;
    }
    if (ctrl && vk == 'E') {
        for (const auto& a : toolbarActions_) if (a.id == "code") { applyAction(a); break; }
        return;
    }
    if (vk == VK_BACK) {
        size_t a = std::min(selAnchor_, caretPos_);
        size_t b = std::max(selAnchor_, caretPos_);
        if (a != b) {
            content_.erase(a, b - a);
            caretPos_ = a;
            selAnchor_ = a;
        } else if (caretPos_ > 0) {
            content_.erase(caretPos_ - 1, 1);
            --caretPos_;
            selAnchor_ = caretPos_;
        }
        undoStack_.push(content_, caretPos_);
        triggerAutosaveTimer();
        InvalidateRect(hwnd_, nullptr, TRUE);
        return;
    }
    if (vk == VK_DELETE) {
        size_t a = std::min(selAnchor_, caretPos_);
        size_t b = std::max(selAnchor_, caretPos_);
        if (a != b) {
            content_.erase(a, b - a);
            caretPos_ = a;
            selAnchor_ = a;
        } else if (caretPos_ < content_.size()) {
            content_.erase(caretPos_, 1);
        }
        undoStack_.push(content_, caretPos_);
        triggerAutosaveTimer();
        InvalidateRect(hwnd_, nullptr, TRUE);
        return;
    }
    if (vk == VK_LEFT && caretPos_ > 0) {
        --caretPos_;
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) selAnchor_ = caretPos_;
        InvalidateRect(hwnd_, nullptr, TRUE);
    } else if (vk == VK_RIGHT && caretPos_ < content_.size()) {
        ++caretPos_;
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) selAnchor_ = caretPos_;
        InvalidateRect(hwnd_, nullptr, TRUE);
    } else if (vk == VK_HOME) {
        size_t ls = content_.rfind('\n', caretPos_ == 0 ? 0 : caretPos_ - 1);
        caretPos_ = (ls == std::string::npos || caretPos_ == 0) ? 0 : ls + 1;
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) selAnchor_ = caretPos_;
        InvalidateRect(hwnd_, nullptr, TRUE);
    } else if (vk == VK_END) {
        size_t le = content_.find('\n', caretPos_);
        caretPos_ = (le == std::string::npos) ? content_.size() : le;
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) selAnchor_ = caretPos_;
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

void NotesView::onLButtonDown(int x, int y) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    if (y >= rc.bottom - kToolbarHeight) {
        for (size_t i = 0; i < toolbarButtonRects_.size(); ++i) {
            POINT pt{x, y};
            if (PtInRect(&toolbarButtonRects_[i], pt)) {
                applyAction(toolbarActions_[i]);
                return;
            }
        }
    }
}

void NotesView::onMouseWheel(int delta, bool horizontal) {
    if (horizontal) {
        toolbarScrollX_ = std::max(0, toolbarScrollX_ - delta / 4);
    } else {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        POINT pt{};
        GetCursorPos(&pt);
        ScreenToClient(hwnd_, &pt);
        if (pt.y >= rc.bottom - kToolbarHeight) {
            toolbarScrollX_ = std::max(0, toolbarScrollX_ - delta / 4);
        } else {
            scrollY_ = std::max(0, scrollY_ - delta / 3);
        }
    }
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void NotesView::onPaint(HDC hdc) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);

    bool dark = core_.settings().snapshot().theme == core::ThemeMode::Dark ||
                (core_.settings().snapshot().theme == core::ThemeMode::System &&
                 core_.platform().isDarkModePreferred());

    COLORREF bg = dark ? RGB(26, 28, 34) : RGB(255, 255, 255);
    COLORREF border = dark ? RGB(64, 72, 90) : RGB(203, 213, 225);
    COLORREF txtMain = dark ? RGB(241, 245, 249) : RGB(15, 23, 42);
    COLORREF txtDim = dark ? RGB(100, 116, 139) : RGB(148, 163, 184);
    COLORREF txtAccent = dark ? RGB(56, 189, 248) : RGB(2, 132, 199);
    COLORREF barBg = dark ? RGB(34, 38, 48) : RGB(241, 245, 249);
    COLORREF btnBg = dark ? RGB(48, 54, 68) : RGB(226, 232, 240);

    HBRUSH hBg = CreateSolidBrush(bg);
    FillRect(hdc, &rc, hBg);
    DeleteObject(hBg);

    SetBkMode(hdc, TRANSPARENT);

    // Subtle header line with note title & save status (no title bar!)
    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, txtDim);
    std::string header = activeNoteId_ + (dirty_ ? "  •  Unsaved" : "  •  Saved") + "   (Esc to save & close)";
    std::wstring wHeader = platform::Win32PlatformServices::utf8ToWide(header);
    RECT hdrRc{kPadding, 8, rc.right - kPadding, 28};
    DrawTextW(hdc, wHeader.c_str(), -1, &hdrRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Integrated Markdown surface
    auto lines = NotesMarkdown::renderDocument(content_, caretPos_);
    int y = 36 - scrollY_;
    for (const auto& line : lines) {
        if (y + kLineHeight < 32) {
            y += kLineHeight;
            continue;
        }
        if (y > rc.bottom - kToolbarHeight - 6) break;

        int x = kPadding;
        for (const auto& sp : line.spans) {
            HFONT f = fontNormal_;
            COLORREF col = txtMain;
            if (sp.dimmedMarker || sp.isSyntaxMarker) {
                col = txtDim;
            } else if (sp.style == SpanStyle::Heading1) {
                f = fontH1_;
            } else if (sp.style == SpanStyle::Heading2 || sp.style == SpanStyle::Heading3) {
                f = fontH2_;
            } else if (sp.style == SpanStyle::Bold) {
                f = fontBold_;
            } else if (sp.style == SpanStyle::Italic) {
                f = fontItalic_;
            } else if (sp.style == SpanStyle::InlineCode || sp.style == SpanStyle::CodeBlock) {
                f = fontMono_;
                col = txtAccent;
            } else if (sp.style == SpanStyle::LinkText) {
                col = txtAccent;
            }
            SelectObject(hdc, f);
            SetTextColor(hdc, col);
            std::wstring wTxt = platform::Win32PlatformServices::utf8ToWide(sp.text);
            SIZE sz{};
            GetTextExtentPoint32W(hdc, wTxt.c_str(), static_cast<int>(wTxt.size()), &sz);
            RECT spRc{x, y, rc.right - kPadding, y + kLineHeight};
            DrawTextW(hdc, wTxt.c_str(), static_cast<int>(wTxt.size()), &spRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            x += sz.cx;
        }
        y += kLineHeight;
    }

    // Bottom horizontally-scrollable formatting toolbar
    RECT barRc{0, rc.bottom - kToolbarHeight, rc.right, rc.bottom};
    HBRUSH hBar = CreateSolidBrush(barBg);
    FillRect(hdc, &barRc, hBar);
    DeleteObject(hBar);

    toolbarButtonRects_.clear();
    int bx = 10 - toolbarScrollX_;
    SelectObject(hdc, fontSmall_);
    for (const auto& act : toolbarActions_) {
        std::wstring wLbl = platform::Win32PlatformServices::utf8ToWide(act.label);
        SIZE sz{};
        GetTextExtentPoint32W(hdc, wLbl.c_str(), static_cast<int>(wLbl.size()), &sz);
        int bw = std::max(36, static_cast<int>(sz.cx) + 18);
        RECT btnRc{bx, rc.bottom - kToolbarHeight + 6, bx + bw, rc.bottom - 6};
        toolbarButtonRects_.push_back(btnRc);

        if (btnRc.right > 0 && btnRc.left < rc.right) {
            HBRUSH hBtn = CreateSolidBrush(btnBg);
            FillRect(hdc, &btnRc, hBtn);
            DeleteObject(hBtn);
            SetTextColor(hdc, txtMain);
            DrawTextW(hdc, wLbl.c_str(), -1, &btnRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        bx += bw + 6;
    }

    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, 0, 0, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}

LRESULT CALLBACK NotesView::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<NotesView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<NotesView*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_NCCALCSIZE:
            if (wParam == TRUE) return 0;  // Remove standard titlebar & frame
            break;
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT rc{};
            GetWindowRect(hwnd, &rc);
            bool left = pt.x < rc.left + kResizeBorder;
            bool right = pt.x >= rc.right - kResizeBorder;
            bool top = pt.y < rc.top + kResizeBorder;
            bool bottom = pt.y >= rc.bottom - kResizeBorder;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
            if (pt.y < rc.top + 28) return HTCAPTION;  // Drag by subtle top strip
            return HTCLIENT;
        }
        case WM_TIMER:
            if (wParam == kAutosaveTimerId) {
                KillTimer(hwnd, kAutosaveTimerId);
                self->saveNow();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_CHAR:
            self->onChar(static_cast<wchar_t>(wParam));
            return 0;
        case WM_KEYDOWN:
            self->onKeyDown(wParam);
            return 0;
        case WM_LBUTTONDOWN:
            self->onLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSEWHEEL:
            self->onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam), false);
            return 0;
        case WM_MOUSEHWHEEL:
            self->onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam), true);
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

}  // namespace cmdpal::features::notes

#endif  // _WIN32
