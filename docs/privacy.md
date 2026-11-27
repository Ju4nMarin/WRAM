# Privacy policy

WRAM does not collect, store or send personal data. It has no telemetry, no analytics, no accounts and no ads.

## What WRAM reads

The metrics come from Windows on the same computer: performance counters, memory status, network and disk counters, battery status and the temperatures exposed by ACPI and by the graphics driver. The values are shown on the taskbar and are not saved.

## What WRAM stores

The settings (metrics, labels, thresholds and appearance) are kept in a `config.ini` file in the user profile:

| Version | Location |
|---|---|
| Executable from GitHub, Scoop or winget | `%APPDATA%\WRAM\config.ini` |
| Microsoft Store | `%LOCALAPPDATA%\Packages\<package>\LocalState\config.ini` |

The settings window runs on Microsoft Edge WebView2 and keeps its cache in the user profile. The file never leaves the computer.

## Network

The Microsoft Store version makes no network connections: updates come from the Store.

The executable from GitHub connects only to the GitHub API and to the GitHub release downloads, to look for and install new versions: when "Check for updates" is pressed and, with "Notify me about new versions" turned on, at most once a day. The request carries no personal data. Details in [security.md](security.md#updates-from-the-app).

## Contact

Questions about privacy: [issues on GitHub](https://github.com/Ju4nMarin/WRAM/issues).
