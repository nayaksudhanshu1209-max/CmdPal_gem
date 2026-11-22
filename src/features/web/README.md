# Web Feature

## Purpose
Provides configurable web search aliases/bangs (`g`, `w`, `yt`), friendly alias chips, saved bookmarks, and arbitrary HTTP/HTTPS URL recognition.

## Security & Browser Policy
- **No embedded browser** (no WebView2, CEF, Chromium, or Electron).
- Strictly validates URLs and rejects dangerous schemes (`javascript:`, `data:`, `vbscript:`, `file:`).
- Opens the Windows default browser **only** on explicit user submission (`Enter` or click).

## Adding / Removing
Registered centrally in `src/bootstrap/FeatureRegistry.cpp`.
