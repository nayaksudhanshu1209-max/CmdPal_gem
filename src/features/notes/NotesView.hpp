#pragma once

#include "core/CoreServices.hpp"
#include "features/notes/NotesCoordinator.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <vector>

namespace cmdpal::features::notes {

class NotesView {
public:
    NotesView(core::CoreServices& core, NotesCoordinator& coordinator);
    ~NotesView();

    bool create(HINSTANCE hInst);
    void openNote(const std::string& noteId);
    void closeAndSave();
    HWND hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void onPaint(HDC hdc);
    void onChar(wchar_t ch);
    void onKeyDown(WPARAM vk);
    void onLButtonDown(int x, int y);
    void onMouseWheel(int delta, bool horizontal);
    void applyAction(const ToolbarAction& action);
    void triggerAutosaveTimer();
    bool saveNow();
    void refreshToolbar();

    core::CoreServices& core_;
    NotesCoordinator& coordinator_;
    HWND hwnd_ = nullptr;
    HFONT fontNormal_ = nullptr;
    HFONT fontBold_ = nullptr;
    HFONT fontItalic_ = nullptr;
    HFONT fontMono_ = nullptr;
    HFONT fontH1_ = nullptr;
    HFONT fontH2_ = nullptr;
    HFONT fontSmall_ = nullptr;

    std::string activeNoteId_;
    std::string content_;
    size_t caretPos_ = 0;
    size_t selAnchor_ = 0;
    bool dirty_ = false;
    int scrollY_ = 0;
    int toolbarScrollX_ = 0;
    NotesCoordinator::UndoStack undoStack_;
    std::vector<ToolbarAction> toolbarActions_;
    std::vector<RECT> toolbarButtonRects_;
};

}  // namespace cmdpal::features::notes

#endif  // _WIN32
