# CmdPal Architecture Overview

## Design Principles
1. **Minimal Entry Point (`src/app/main.cpp`)**: Contains only `wWinMain`/`main` delegating to `cmdpal::bootstrap::runApplication`.
2. **Central Composition (`src/bootstrap/`)**:
   - `Bootstrap.cpp` / `BootstrapApp.cpp` construct `IPlatformServices`, `SettingsStore`, `UsageTracker`, `EventBus`, and `CoreServices`.
   - `FeatureRegistry.cpp` is the **single documented integration point** where all feature modules are instantiated, initialized, and wired to `PaletteCoordinator` via public contracts (`ISearchProvider`, `IWebService`, `IFileSearchService`, `IQuickFoldersService`).
3. **Shared Core (`src/core/`)**:
   - `Contracts.hpp`: `IFeature`, `ISearchProvider`, `SearchResult`, `QueryContext`, `ActionResult`, `SettingDescriptor`.
   - `IPlatformServices.hpp`: Abstracts OS calls (`openExternalUrl`, `launchExecutable`, `openShellItem`, `getKnownFolders`, monitor geometry, `setStartupEnabled`) so core/feature logic is 100% unit-testable without Win32 UI.
   - `Json.hpp/.cpp`, `SettingsStore.hpp/.cpp`, `UsageTracker.hpp/.cpp`, `EventBus.hpp`, `SearchMatch.hpp`.
4. **Feature-First Modularity (`src/features/<name>/`)**:
   - Each feature owns its Contract, Models, Coordinator (logic), View (Win32 UI where applicable), and `README.md`.
   - Features never call another feature's private implementation; communication uses public contracts and `EventBus`.

## Adding or Removing a Feature
- **Add**: Create `src/features/<name>/` with Contract, Coordinator, View (optional), tests, and `README.md`, then register it in `FeatureRegistry::registerAllFeatures` and add its `.cpp` to `CMakeLists.txt`.
- **Remove**: Remove its registration in `FeatureRegistry::registerAllFeatures` and `CMakeLists.txt`, and delete `src/features/<name>/`.
