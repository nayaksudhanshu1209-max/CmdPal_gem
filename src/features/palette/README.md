# Palette Feature

## Purpose
Core command palette window, unified search aggregator, ranking engine, keyboard navigation, and multi-monitor work-area placement.

## Keyboard Behavior
- `Up` / `Down`: Move selection through results.
- `Tab` / `Shift+Tab`: Cycle selection forward/backward.
- `Page Up` / `Page Down`: Jump by 5 results.
- `Enter`: Execute selected result and hide palette.
- `Escape`: Hide palette.
- **Explicitly excluded**: `Alt+1` through `Alt+9` shortcuts and right-side number hints.

## Adding / Removing
Registered centrally in `src/bootstrap/FeatureRegistry.cpp`.
