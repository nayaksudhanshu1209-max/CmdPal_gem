# CmdPal

CmdPal is a lightweight, keyboard-first Windows command palette built in **C++20** with native **Win32 APIs** and **zero external third-party dependencies**. It is designed to start quickly and run smoothly on modest Windows 10 and Windows 11 hardware (including 4 GB RAM, older Intel i3-class CPUs, and mechanical HDDs) without requiring .NET or an embedded browser runtime.

## Key Features
- **Command Palette**: Configurable global shortcut (default `Ctrl+Alt+Space`), always-on-top while open, auto-hide on focus loss or launch, multi-monitor & DPI work-area clamping, optional cursor-position placement, compact/comfortable density, and system/light/dark themes.
- **Application Search**: Caches Start Menu shortcuts without administrator privileges; supports hiding and restoring applications.
- **Commands & Tasks**: Built-in commands plus validated user-defined commands executed via structured process arguments (`CreateProcessW`).
- **Web Bangs, Bookmarks & URLs**: Configurable search aliases (`g`, `w`, `yt`) with friendly chips, saved bookmarks, and strict `http://`/`https://` URL validation that hands off to the Windows default browser only on explicit submission.
- **File Search & Indexing**: Root-scoped background indexer with independently toggleable Desktop/Documents/Downloads defaults, custom roots, pause/resume/rebuild, symlink/junction boundary safety, and persisted `file_index.json`.
- **Quick Folders**: Independent folder shortcuts with add/edit/remove/enable/disable/reorder controls.
- **Markdown Notes**: Standalone, frameless, resizable editor window with integrated single-surface Markdown rendering, caret-sensitive syntax visibility, debounced autosave, Escape-to-save-and-close, and a horizontally scrollable bottom formatting toolbar ordered per note.
- **Settings GUI & Search**: Graphical Settings window connected to `settings.json`, searchable directly from the palette.

## Building in the Cloud (GitHub Actions)
1. Upload this repository to GitHub.
2. Open the **Actions** tab and run **Build and Test CmdPal (Windows)** (or push to `main`).
3. Download the **CmdPal-Portable-win64** artifact (unsigned portable CI build containing `CmdPal.exe`, `sbom.json`, `SHA256SUMS.txt`, `LICENSE`, `README.txt`, and `SECURITY_AND_PRIVACY.md`).

## Local Build & Test (Optional)
- **Windows (MSVC + CMake 3.20+)**:
  ```powershell
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Release
  ctest --test-dir build -C Release --output-on-failure
  ```
- **Core/Feature Tests on Linux/macOS (g++ / clang++ C++20)**:
  ```bash
  g++ -std=c++20 -O2 -Wall -Wextra -Wno-maybe-uninitialized -I. -Isrc \
    src/core/*.cpp src/bootstrap/Bootstrap.cpp src/bootstrap/FeatureRegistry.cpp \
    src/features/*/*.cpp tests/*.cpp -pthread -o cmdpal_tests && ./cmdpal_tests
  ```
