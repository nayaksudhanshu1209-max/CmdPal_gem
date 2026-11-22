# Quick Folders Feature

## Purpose
Provides fast access to user-configured folder shortcuts with add, edit, remove, enable/disable, and reorder controls.

## Relationship to File Search
Quick Folders are independent from File Search roots. Removing a Quick Folder never removes a File Search root. A Quick Folder may optionally be marked `indexAsRoot = true` to include it in file indexing.

## Adding / Removing
Registered centrally in `src/bootstrap/FeatureRegistry.cpp`.
