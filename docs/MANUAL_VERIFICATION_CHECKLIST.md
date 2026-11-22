# Manual Windows Desktop Verification Checklist

Because the GitHub Actions `windows-2022` runner is non-interactive and this source package was authored on a Linux VM without a Windows desktop session, execute these interactive UI checks on a Windows 10/11 machine using the built `CmdPal.exe`:

1. Launch `CmdPal.exe`; verify tray icon appears and no duplicate processes exist.
2. Press `Ctrl+Alt+Space`; verify palette opens centered, stays on top, and auto-hides when clicking outside.
3. Verify `Alt+1`–`Alt+9` do nothing and no number hints appear on result rows.
4. Search for an installed Start Menu app (e.g. Notepad) and press `Enter`; verify it launches and palette hides.
5. Right-click a result -> Hide Application / Hide Result; open Settings -> Hidden Items -> Restore.
6. Type `g test query`; verify `[Google Search]` chip appears, text is preserved, and pressing `Enter` opens the default browser.
7. Type `https://example.com/home` and press `Enter`; verify default browser opens.
8. Open Settings -> File Search & Folders; toggle Desktop/Documents/Downloads independently and add/remove a custom root.
9. Run `n` or `Open Notes`; verify frameless, resizable, non-topmost note window opens, stays open when losing focus, formats Markdown with caret-sensitive syntax dimming, scrolls the bottom toolbar horizontally, and saves & closes on `Escape`.
10. Toggle "Run CmdPal at Windows startup" in Settings and verify `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\CmdPal`.
