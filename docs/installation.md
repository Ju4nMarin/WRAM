# Installation

WRAM is a single portable executable. It has no installer and writes nothing outside your user profile.

## Requirements

| | |
|---|---|
| System | Windows 10 version 1903 or later, or Windows 11 |
| Architecture | x64 |
| Permissions | Standard user, no administrator |
| Settings window | Microsoft Edge WebView2 Runtime |

WebView2 ships with Windows 11 and with up-to-date Windows 10. If it is missing, WRAM shows a message when the settings window opens; the taskbar strip keeps working. Install it from Microsoft: <https://developer.microsoft.com/microsoft-edge/webview2/> ("Evergreen Bootstrapper").

GPU temperature needs a WDDM 2.4 or later driver (Windows 10 1803 onwards with current NVIDIA, AMD or Intel drivers).

## Steps

1. Download `WRAM.exe` from the latest release: <https://github.com/Ju4nMarin/WRAM/releases/latest>.
2. Create a permanent folder for it, for example:

   ```
   %LOCALAPPDATA%\Programs\WRAM
   ```

   Startup with Windows stores the executable path, so Downloads is not a good place for it.
3. Move `WRAM.exe` to that folder and run it.
4. The first time, the settings window opens and the strip appears next to the tray.

### SmartScreen warning

The executable is not signed with a commercial code-signing certificate, so the first run may show "Windows protected your PC". To continue, select "More info" and then "Run anyway". The file properties (right-click, Properties, Details) show the author and version.

To check that the file is the published one, compare its hash with the one on the release page:

```powershell
Get-FileHash .\WRAM.exe -Algorithm SHA256
```

With the GitHub CLI you can also verify that it was built by this repository's CI:

```
gh attestation verify WRAM.exe --repo Ju4nMarin/WRAM
```

## Start with Windows

In the settings window, Settings section, turn on "Start with Windows". It is registered under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` for your user only.

## Update

1. Right-click the strip or the tray icon and choose "Exit WRAM".
2. Replace `WRAM.exe` with the new version in the same folder.
3. Run it. Your settings are kept.

## Uninstall

1. In Settings, turn off "Start with Windows".
2. Exit WRAM from the strip or tray menu.
3. Delete `WRAM.exe` and, to remove the settings too, these folders:

   | Folder | Contents |
   |---|---|
   | `%APPDATA%\WRAM` | `config.ini` |
   | `%LOCALAPPDATA%\WRAM` | Settings window data (WebView2) |

## Build from source

See [development.md](development.md).
