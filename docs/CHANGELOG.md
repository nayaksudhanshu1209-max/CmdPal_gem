# Changelog

## 1.0.0 — Initial Greenfield Release
- Built CmdPal from zero in C++20 + native Win32 with zero third-party dependencies.
- Implemented central `FeatureRegistry` and `Bootstrap` composition layer.
- Implemented all 8 feature modules: `palette`, `apps`, `commands`, `web`, `filesearch`, `quickfolders`, `notes`, and `settings`.
- Generated multi-resolution `assets/cmdpal.ico` and Windows 10/11 DPI-aware `assets/app.manifest`.
- Added unit, integration, and excluded-feature regression tests (`tests/`) plus `scripts/check_excluded_features.py`.
- Added GitHub Actions workflow `.github/workflows/build-and-test.yml` for Windows MSVC builds, DLL dependency audits, SBOM, SHA-256 checksums, and portable ZIP packaging.
