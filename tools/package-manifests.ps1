# Genera los manifiestos de Scoop y winget de una versión publicada.
# Uso: tools/package-manifests.ps1 -Version 1.7.4 -Sha256 <hash de WRAM.exe> -OutDir build/packages
param(
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$Sha256,
    [Parameter(Mandatory)][string]$OutDir,
    [string]$Repo = "Ju4nMarin/WRAM"
)
$ErrorActionPreference = "Stop"
if ($Version -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}$') { throw "Versión no válida: $Version" }
if ($Sha256 -notmatch '^[0-9A-Fa-f]{64}$') { throw "SHA-256 no válido: $Sha256" }
$Sha256 = $Sha256.ToUpperInvariant()

$id = "Ju4nMarin.WRAM"
$url = "https://github.com/$Repo/releases/download/v$Version/WRAM.exe"
$site = "https://github.com/$Repo"
# Sin caracteres fuera de ASCII en el script: Windows PowerShell 5 lee el archivo sin BOM como ANSI.
$publisher = "Juan Guillermo Mar$([char]0x00ED)n C$([char]0x00F3)rdoba"
$desc = "System monitor for the Windows taskbar: CPU, RAM, GPU, temperatures and network next to the tray."
$utf8 = New-Object System.Text.UTF8Encoding($false)
New-Item -ItemType Directory -Force $OutDir | Out-Null

# Scoop: se instala con `scoop install https://github.com/<repo>/releases/latest/download/wram.json`
# y `scoop update wram` vuelve a leer esa URL, que siempre apunta a la última versión.
$scoop = [ordered]@{
    version     = $Version
    description = $desc
    homepage    = $site
    license     = "MIT"
    url         = $url
    hash        = $Sha256.ToLowerInvariant()
    bin         = "WRAM.exe"
    shortcuts   = @(, @("WRAM.exe", "WRAM"))
    checkver    = "github"
    autoupdate  = [ordered]@{
        url  = "https://github.com/$Repo/releases/download/v`$version/WRAM.exe"
        hash = [ordered]@{ url = "`$url.sha256" }
    }
}
[IO.File]::WriteAllText((Join-Path $OutDir "wram.json"), ($scoop | ConvertTo-Json -Depth 5) + "`n", $utf8)

# winget: manifiesto de varios archivos (versión, instalador y textos), esquema 1.10.
$wingetDir = Join-Path $OutDir "winget"
New-Item -ItemType Directory -Force $wingetDir | Out-Null
$schema = "1.10.0"
$files = @{
    "$id.yaml"                = @"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.version.$schema.schema.json
PackageIdentifier: $id
PackageVersion: $Version
DefaultLocale: en-US
ManifestType: version
ManifestVersion: $schema
"@
    "$id.installer.yaml"      = @"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.installer.$schema.schema.json
PackageIdentifier: $id
PackageVersion: $Version
InstallerType: portable
Commands:
- wram
MinimumOSVersion: 10.0.19041.0
Installers:
- Architecture: x64
  InstallerUrl: $url
  InstallerSha256: $Sha256
ManifestType: installer
ManifestVersion: $schema
"@
    "$id.locale.en-US.yaml"   = @"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.defaultLocale.$schema.schema.json
PackageIdentifier: $id
PackageVersion: $Version
PackageLocale: en-US
Publisher: $publisher
PublisherUrl: https://github.com/Ju4nMarin
PublisherSupportUrl: $site/issues
Author: $publisher
PackageName: WRAM
PackageUrl: $site
License: MIT
LicenseUrl: $site/blob/main/LICENSE
ShortDescription: "$desc"
Description: "Shows up to 10 live metrics (CPU, RAM, GPU, temperatures, disk, network, battery) on the Windows taskbar, next to the tray. Native C++ in a single executable of about 430 KB, in six languages."
Moniker: wram
Tags:
- system-monitor
- taskbar
- hardware-monitor
- cpu
- ram
- gpu
- temperature
ReleaseNotesUrl: $site/blob/main/CHANGELOG.md
ManifestType: defaultLocale
ManifestVersion: $schema
"@
}
foreach ($name in $files.Keys) {
    [IO.File]::WriteAllText((Join-Path $wingetDir $name), ($files[$name] -replace "`r`n", "`n") + "`n", $utf8)
}
Get-ChildItem -Recurse $OutDir -File | ForEach-Object { $_.FullName }
