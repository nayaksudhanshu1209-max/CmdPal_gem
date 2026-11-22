# Apps Feature

## Purpose
Discovers installed Windows applications from user and system Start Menu folders without administrator privileges, caches the results in memory, and launches selected applications via the Windows shell.

## Architecture
- `AppsContract.hpp`: Public `IAppsService` interface.
- `AppsModels.hpp`: `AppEntry` data model.
- `AppsCoordinator.hpp/.cpp`: Implements `IFeature`, `ISearchProvider`, and `IAppsService`.
- Supports hiding individual applications (`UsageTracker::hideApp`) without modifying or uninstalling them.

## Adding / Removing
Registered centrally in `src/bootstrap/FeatureRegistry.cpp`. Remove its registration entry and delete `src/features/apps/` to remove the feature cleanly.
