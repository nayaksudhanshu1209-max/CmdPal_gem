# Settings Feature

## Purpose
Provides the graphical Settings window and exposes all registered `SettingDescriptor` items for direct search and navigation from the command palette.

## Persistence
Settings are stored in `settings.json` via `SettingsStore` with schema validation and automatic `.bak` backup recovery on malformed files.

## Adding / Removing
Registered centrally in `src/bootstrap/FeatureRegistry.cpp`.
