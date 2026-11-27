# Microsoft Store

The Store version is the same `WRAM.exe`, packaged as MSIX. At startup WRAM detects that it runs from a package (`GetCurrentPackageFullName`) and changes three things:

| | Executable from GitHub | Microsoft Store |
|---|---|---|
| Updates | Built-in updater (GitHub) | The Store installs them; the update button and the daily notice are hidden |
| Startup with Windows | `HKCU\...\Run` | Package startup task `WRAMStartup` (`windows.startupTask`) |
| Settings file | `%APPDATA%\WRAM\config.ini` | `%LOCALAPPDATA%\Packages\<package>\LocalState\config.ini` |

The Store signs the package when it publishes it, so the Store version does not show the SmartScreen warning.

## Package

`tools/package-msix.ps1` builds the package from `build/WRAM.exe`:

- `AppxManifest.xml` with the identity from Partner Center, x64, Windows 10 2004 (10.0.19041) or later, the six interface languages, the startup task and the `runFullTrust` capability.
- Logos generated from `res/wram.ico`: `StoreLogo`, `Square44x44Logo` (with target sizes 16, 24, 32, 48 and 256, plated and unplated), `Square150x150Logo`, `Wide310x150Logo` and `Square310x310Logo`.
- `MakeAppx pack` from the newest Windows SDK installed. The package is not signed.

```powershell
build.cmd
powershell -ExecutionPolicy Bypass -File tools\package-msix.ps1 -Version 1.7.4 -IdentityName "<Package/Identity/Name>" -Publisher "CN=<Package/Identity/Publisher>" -OutDir build\msix
```

The version has three parts; the package adds `.0`, as the Store requires.

## Local test

With Developer Mode on, the package folder can be registered without signing:

```powershell
powershell -ExecutionPolicy Bypass -File tools\package-msix.ps1 -Version 1.7.4 -IdentityName Test.WRAM -Publisher "CN=WRAM Test" -LayoutOnly
Add-AppxPackage -Register build\msix\layout\AppxManifest.xml
# WRAM shows up in Start; to remove it:
Get-AppxPackage Test.WRAM | Remove-AppxPackage
```

Only one WRAM runs at a time: close the portable executable before opening the packaged one.

## CI

The build job builds a package with a test identity on every push, to catch packaging errors. In the release job, when the repository variables `MSIX_IDENTITY_NAME` and `MSIX_PUBLISHER` exist, it builds the package with the Store identity and uploads it as the `WRAM-msix-<tag>` artifact. That `.msix` is the file uploaded to Partner Center.

## Publishing in Partner Center

1. Create the developer account at [Microsoft Partner Center](https://partner.microsoft.com/dashboard/registration/apps), individual type (no fee).
2. Reserve the name WRAM in Apps and games > New product > MSIX or PWA app.
3. In Product management > Product identity, copy `Package/Identity/Name` and `Package/Identity/Publisher` into the repository variables `MSIX_IDENTITY_NAME` and `MSIX_PUBLISHER`.
4. Publish a version tag, download the `WRAM-msix-<tag>` artifact from the workflow run and upload the `.msix` in Packages.
5. Fill in Pricing (free), Properties (category Utilities and tools), Age ratings and Store listings.
6. In Submission options, justify `runFullTrust` with the text below.

Justification for `runFullTrust`:

> WRAM is a native Win32 desktop app (C++, Direct2D) that draws a small metrics strip next to the notification area of the Windows taskbar. It needs full trust to read system performance counters (PDH), GPU information through D3DKMT and ACPI thermal zones, and to place its window on the taskbar. It does not require administrator rights, installs no drivers or services and makes no network connections in the Store version.

Privacy policy URL: `https://github.com/Ju4nMarin/WRAM/blob/main/docs/privacy.md`.

## Pending

- Partner Center account and reservation of the name WRAM.
- Repository variables `MSIX_IDENTITY_NAME` and `MSIX_PUBLISHER`.
- First submission and certification.
