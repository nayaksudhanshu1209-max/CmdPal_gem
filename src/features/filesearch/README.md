# File Search & Indexing Feature

## Purpose
Indexes filenames and lightweight metadata (path, extension, size, modified time) strictly within user-selected roots. Desktop, Documents, and Downloads defaults are independently toggleable and removable.

## Safety & Performance
- Never scans the entire disk.
- Never follows symlinks or directory junctions out of the configured root.
- Starts background indexing lazily after the palette is responsive, with throttling, pause/resume, and persisted incremental cache (`file_index.json`).

## Adding / Removing
Registered centrally in `src/bootstrap/FeatureRegistry.cpp`.
