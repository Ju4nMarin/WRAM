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

Each release includes the SHA-256 hash of `WRAM.exe`. The file properties show the author, version and copyright in the version metadata.

## Reporting a vulnerability

Open a private advisory at <https://github.com/Ju4nMarin/WRAM/security/advisories/new> with the steps to reproduce it.

## Pending

- Authenticode signing with a certificate from a recognized authority.
