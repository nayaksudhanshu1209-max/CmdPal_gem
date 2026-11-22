# Notes Feature

## Purpose
Standalone, frameless, resizable Markdown note editor that coexists with the command palette, stays open on focus loss, and closes on Escape after ensuring changes are saved.

## Architecture
- `NotesModels.hpp`: Note metadata, toolbar action definitions, and undo snapshot structures.
- `NotesMarkdown.hpp/.cpp`: Integrated Markdown parser, caret-sensitive syntax visibility (`dimmedMarker` when caret is inside a span, hidden when outside), source-to-visual mapping, and Enter-key list continuation.
- `NotesCoordinator.hpp/.cpp`: File persistence (`.md`), debounced autosave coordination, undo/redo stack, and per-note toolbar action ordering.
- `NotesView.hpp/.cpp`: Win32 frameless resizable editor window with horizontally scrollable bottom formatting toolbar.

## Adding / Removing
Registered centrally in `src/bootstrap/FeatureRegistry.cpp`.
