# Commands Feature

## Purpose
Provides built-in CmdPal actions (Settings, Notes, Rebuild Index, Reload Apps, Quit) and user-defined commands/tasks launched via safe structured process arguments (`CreateProcessW` / `IPlatformServices::launchExecutable`) without shell string concatenation.

## Adding / Removing
Registered centrally in `src/bootstrap/FeatureRegistry.cpp`.
