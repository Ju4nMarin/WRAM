# Security

## Permissions

- Runs as the user (`requestedExecutionLevel asInvoker`). It does not ask for administrator rights and installs no drivers or services.
- Writes only to `%APPDATA%\WRAM`, `%LOCALAPPDATA%\WRAM` and, when enabled, `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.
- Opens no network connections. The settings window loads from the executable itself.

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

## Reporting a vulnerability

Open a private advisory at <https://github.com/Ju4nMarin/WRAM/security/advisories/new> with the steps to reproduce it.

## Pending

- Authenticode signing with a certificate from a recognized authority.
