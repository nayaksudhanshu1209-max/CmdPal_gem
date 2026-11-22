# CmdPal Decision Log

## 1. Language & Stack: C++20 + Native Win32 (Zero Third-Party Dependencies)
- **Evaluated**: C++20 (native Win32) vs. Rust (`windows-rs` + UI crate).
- **Decision**: **C++20 with native Win32 APIs and static MSVC CRT**, with zero external third-party libraries.
- **Rationale**:
  - **Startup & Footprint**: Native C++20 Win32 starts in tens of milliseconds with minimal idle RAM (~5–15 MB) and no .NET runtime, GC, or browser runtime.
  - **Windows 10/11 Integration**: `RegisterHotKey`, `CreateProcessW`, `ShellExecuteW`, `SHGetKnownFolderPath`, `MonitorFromPoint`, `DwmSetWindowAttribute`, and `HKCU\...\Run` are native C/C++ APIs.
  - **Build & Antivirus Transparency**: Zero external packages means no third-party DLLs, no npm/Cargo dependency tree to audit, deterministic builds on `windows-2022` GitHub Actions runners, and a clean static import table.
  - **Testability**: Isolating Win32 calls behind `IPlatformServices` allows all core and feature logic to be compiled and unit/integration tested on both Linux (`g++ -std=c++20`) and Windows (MSVC).

## 2. Persistence Format: Human-Readable JSON (`settings.json`, `usage.json`, `file_index.json`)
- Implemented a lightweight, depth-limited RFC 8259 JSON parser (`src/core/Json.hpp/.cpp`) with atomic temp-file-and-rename writes and `.bak` recovery on malformed settings files.

## 3. Global Shortcut Default: `Ctrl+Alt+Space`
- Chosen to avoid conflicting with Windows' built-in `Alt+Space` system window menu, while remaining user-configurable in Settings.
