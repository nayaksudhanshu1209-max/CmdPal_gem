# CmdPal Feature Catalogue

| Feature ID | Module Path | Description |
| :--- | :--- | :--- |
| `palette` | `src/features/palette/` | Always-on-top command palette window, global hotkey, unified search ranking, keyboard navigation (`Up/Down/Tab/Shift+Tab/PageUp/PageDown/Enter/Esc`), work-area clamping. |
| `apps` | `src/features/apps/` | Start Menu application discovery, in-memory caching, launching, and hide/restore support. |
| `commands` | `src/features/commands/` | Built-in commands and validated user-defined commands with structured argument execution. |
| `web` | `src/features/web/` | Configurable web bangs (`g`, `w`, `yt`), friendly chips, bookmarks, and strict `http://`/`https://` URL handoff to the default browser. |
| `filesearch` | `src/features/filesearch/` | Root-scoped throttled background file indexer with persisted cache, pause/resume/rebuild, and symlink boundary safety. |
| `quickfolders` | `src/features/quickfolders/` | Independent folder shortcuts with add/edit/remove/enable/disable/reorder and optional `indexAsRoot` link. |
| `notes` | `src/features/notes/` | Standalone frameless resizable Markdown editor with caret-sensitive syntax visibility, autosave, Escape-to-save-and-close, and scrollable per-note toolbar. |
| `settings` | `src/features/settings/` | Graphical Settings window and palette-searchable settings navigation. |

## Explicitly Excluded Features
Per specification, the following are permanently excluded and verified absent by automated tests and `scripts/check_excluded_features.py`:
- `Alt+1` through `Alt+9` result shortcuts and number hints.
- Windows switcher / Alt-Tab replacement.
- Unit conversion, number-base conversion, colour converter, and general text tools.
- Embedded browser (WebView2, CEF, Chromium, Electron, or browser extensions).
