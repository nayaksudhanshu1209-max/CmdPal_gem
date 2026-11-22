#!/usr/bin/env python3
"""Static regression check verifying excluded features and browser engines are absent."""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SCAN_DIRS = [ROOT / "src", ROOT / "CMakeLists.txt"]

FORBIDDEN_PATTERNS = [
    "WebView2",
    "ICoreWebView2",
    "cef_app",
    "libcef",
    "electron",
    "UnitConverter",
    "NumberBaseConverter",
    "ColorConverter",
    "ColourConverter",
    "WindowSwitcher",
    "Win32WindowSwitcher",
    "TextTools",
    "uBlock",
]

violations = []

def scan_file(path: pathlib.Path):
    text = path.read_text(encoding="utf-8", errors="ignore")
    lower = text.lower()
    for pat in FORBIDDEN_PATTERNS:
        if pat.lower() in lower:
            violations.append(f"{path.relative_to(ROOT)}: found forbidden pattern '{pat}'")

for entry in SCAN_DIRS:
    if entry.is_file():
        scan_file(entry)
    elif entry.is_dir():
        for p in entry.rglob("*"):
            if p.is_file() and p.suffix in {".hpp", ".h", ".cpp", ".c", ".txt", ".rc"}:
                scan_file(p)

if violations:
    print("EXCLUDED FEATURE CHECK FAILED:")
    for v in violations:
        print("  -", v)
    sys.exit(1)

print("Excluded feature static check PASSED (0 violations across src/ and CMakeLists.txt).")
