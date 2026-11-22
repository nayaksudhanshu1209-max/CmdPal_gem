# Security, Privacy, and Antivirus Transparency

## Local-First & Offline Operation
- CmdPal makes **zero** network requests at startup and contains **no** telemetry, analytics, auto-updaters, or remote configuration.
- Internet access occurs **only** when the user explicitly submits a web search, bookmark, or `http://`/`https://` URL, which is handed off to the Windows default browser via `ShellExecuteW`.

## Safe Execution & Input Handling
- **User-Defined Commands**: Validated before saving; executed via `CreateProcessW` with structured executable and argument quoting (`quoteWin32Arg`). Never concatenated into `cmd.exe /c`.
- **URL Validation**: Strictly allows `http://` and `https://` schemes; rejects `javascript:`, `data:`, `vbscript:`, and other unsafe schemes.
- **File Indexing**: Restricted to user-configured roots. Never follows symlinks or directory junctions (`is_symlink` check + `isPathInsideRoot` boundary verification). Never executes files during indexing.
- **Least Privilege**: Manifest specifies `asInvoker`. Standard operation and `HKCU\...\Run` startup registration do not require administrator privileges.

## Build Transparency & False-Positive Reporting
- CmdPal uses **no packers, obfuscators, process injection, or hidden persistence**.
- GitHub Actions generates `sbom.json`, `SHA256SUMS.txt`, and runs `dumpbin /dependents` to verify no browser or `.NET` runtimes are linked.
- **Note on Unsigned CI Builds**: Because the CI artifact is unsigned unless valid signing credentials are provided, Windows SmartScreen or heuristic scanners may warn on a newly built `.exe`. If a reputable antivirus flags a clean build, verify the SHA-256 hash against `SHA256SUMS.txt` and submit the clean binary + build logs to Microsoft Security Intelligence (`https://www.microsoft.com/en-us/wdsi/filesubmission`) as a false-positive developer submission.
