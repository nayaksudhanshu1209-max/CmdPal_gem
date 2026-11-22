#pragma once

#include "core/CoreServices.hpp"
#include "features/palette/PaletteCoordinator.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace cmdpal::features::palette {

class PaletteView {
public:
    PaletteView(core::CoreServices& core, PaletteCoordinator& coordinator);
    ~PaletteView();

    bool create(HINSTANCE hInst);
    void show();
    void hide();
    void toggle();
    void applyHotkey();
    HWND hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    void onPaint(HDC hdc);
    void onQueryChanged();
    void onCommandExecute(int index);
    void updateLayout();
    bool isDark() const;

    core::CoreServices& core_;
    PaletteCoordinator& coordinator_;
    HWND hwnd_ = nullptr;
    HWND editHwnd_ = nullptr;
    HFONT fontTitle_ = nullptr;
    HFONT fontSub_ = nullptr;
    HFONT fontInput_ = nullptr;
    HBRUSH bgBrush_ = nullptr;
    int hoverIndex_ = -1;
    bool suppressEditChange_ = false;
    bool trayAdded_ = false;
};

}  // namespace cmdpal::features::palette

#endif  // _WIN32
