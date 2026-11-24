# Security

## Permissions

- Runs as the user (`requestedExecutionLevel asInvoker`). It does not ask for administrator rights and installs no drivers or services.
- Writes only to `%APPDATA%\WRAM`, `%LOCALAPPDATA%\WRAM`, when enabled to `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, and during an update to the folder of `WRAM.exe`.
- Opens network connections only when "Check for updates" is pressed (see [Updates from the app](#updates-from-the-app)). The settings window loads from the executable itself.

## Settings window (WebView2)

- Content: a single HTML file embedded as a resource, with the CSP `default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; img-src data:`.
- Navigation: only the initial load of that HTML is allowed; any other navigation is cancelled.
- New windows and permission requests (camera, location, notifications and others) are denied.
- Disabled: developer tools, context menus, zoom, autofill, password saving, browser shortcuts and host objects.
- External links never leave the page: the interface asks for a target by name (`repo`, `license`, `config`) and the program opens only URLs and folders fixed in the code.

## Input

- Messages from the interface are capped at 32 KB.
- The configuration arrives as INI and goes through `ConfigCodec`: allow lists for metric keys and fonts, ranges for numbers, a fixed format for colors, labels without control characters or separators, and at most 10 unique positions.
- `config.ini` is rejected above 64 KB and written atomically (temporary file and `MoveFileEx`).

## Binary hardening

| Option | Effect |
|---|---|
| `/GS`, `/sdl` | Buffer overrun checks and security warnings as errors |
| `/guard:cf` | Control Flow Guard |
| `/DYNAMICBASE`, `/HIGHENTROPYVA` | 64-bit ASLR |
| `/NXCOMPAT` | DEP |
| `/CETCOMPAT` | Hardware shadow stack (CET) |
| `/DEPENDENTLOADFLAG:0x800` | Imported DLLs are searched only in System32 |
| Static CRT | No dependency on `vcruntime140.dll` |

The process heap terminates on corruption (`HeapEnableTerminationOnCorruption`) and only one instance runs per session.

## Dependencies and supply chain

| Dependency | Version | How it is pinned |
|---|---|---|
| Microsoft.Web.WebView2 SDK | 1.0.4370.26 | Downloaded by CMake from NuGet and checked against its SHA-256 |
| actions/checkout | v7.0.1 | Full commit SHA, without persisted credentials |
| actions/upload-artifact | v7.0.2 | Full commit SHA |
| actions/attest-build-provenance | v4.2.2 | Full commit SHA |
| mikepenz/action-junit-report | v6.7.1 | Full commit SHA |

Pinning actions by commit SHA means a moved or compromised tag cannot change what the workflow runs. Dependabot (`.github/dependabot.yml`) checks the actions every week and opens a pull request to `develop` when there is a new version. The workflow token is read-only except in the release job, which only gets the permissions it needs to publish and attest.

## Executable integrity

Releases are built by the CI workflow from the tagged commit, not on a developer machine. Each release includes:

- The SHA-256 hash of `WRAM.exe`.
- A build provenance attestation signed with Sigstore through GitHub. It proves the file was produced by this repository's workflow from that commit. Verify it with the GitHub CLI:

  ```
  gh attestation verify WRAM.exe --repo Ju4nMarin/WRAM
  ```

The file properties (Details tab) show the version metadata:

| Field | Value |
|---|---|
| Product name | WRAM |
| File description | WRAM - monitor del sistema en la barra de tareas |
| Company | Juan Guillermo Marín Córdoba |
| Copyright | © 2026 Juan Guillermo Marín Córdoba. Licencia MIT. |
| Comments | Desarrollado por Juan Guillermo Marín Córdoba. https://github.com/Ju4nMarin/WRAM |
| Original file name | WRAM.exe |

## Updates from the app

"Check for updates" in Settings is the only feature that uses the network, and it runs only when the button is pressed, on a background thread.

- It asks `https://api.github.com/repos/Ju4nMarin/WRAM/releases/latest` for the latest version. The tag must be `vX.Y.Z` with digits only; anything else is rejected.
- It downloads `WRAM.exe` and `WRAM.exe.sha256` from that release on `github.com`, over HTTPS with TLS 1.2 or 1.3 and certificate validation by WinHTTP. Redirects to HTTP are refused, and the download is capped at 16 MB.
- The file is installed only if its SHA-256 matches the published hash, it starts with a PE header and its version resource matches the tag.
- The current executable is renamed to `WRAM.exe.old`, the new one takes its place, and the app restarts after releasing its single-instance lock. If any step fails, the original file is restored. `WRAM.exe.old` is deleted on the next start.
- No personal data is sent. The requests carry the user agent `WRAM-Updater`.

## Reporting a vulnerability

Open a private advisory at <https://github.com/Ju4nMarin/WRAM/security/advisories/new> with the steps to reproduce it.

## Pending

- Authenticode signing with a certificate from a recognized authority.
