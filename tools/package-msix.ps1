# Empaqueta WRAM.exe como MSIX para Microsoft Store.
# Uso: tools/package-msix.ps1 -Version 1.7.4 -IdentityName <Package/Identity/Name> -Publisher "CN=..." -OutDir build/msix
# IdentityName y Publisher son los que asigna Partner Center al reservar el nombre (Identidad del producto).
# El paquete sale sin firmar: Microsoft Store lo firma al publicarlo.
param(
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$IdentityName,
    [Parameter(Mandatory)][string]$Publisher,
    [string]$PublisherDisplayName = "",
    [string]$Exe = "build/WRAM.exe",
    [string]$Icon = "res/wram.ico",
    [string]$OutDir = "build/msix",
    # Solo arma la carpeta del paquete, sin MakeAppx (para registrarla en modo desarrollador).
    [switch]$LayoutOnly
)
$ErrorActionPreference = "Stop"
if ($Version -notmatch '^\d{1,4}\.\d{1,4}\.\d{1,4}$') { throw "Versión no válida: $Version" }
if ($Publisher -notmatch '^CN=') { throw "Publisher debe empezar con CN=, como lo muestra Partner Center" }
# Sin caracteres fuera de ASCII en el script: Windows PowerShell 5 lee el archivo sin BOM como ANSI.
if (-not $PublisherDisplayName) { $PublisherDisplayName = "Juan Guillermo Mar$([char]0x00ED)n C$([char]0x00F3)rdoba" }
$esc = [Security.SecurityElement]

$layout = Join-Path $OutDir "layout"
$assets = Join-Path $layout "Assets"
Remove-Item -Recurse -Force $layout -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $assets | Out-Null
Copy-Item $Exe (Join-Path $layout "WRAM.exe")

# Logos del paquete a partir del icono de la app, con fondo transparente.
Add-Type -AssemblyName System.Drawing
$icoBytes = [IO.File]::ReadAllBytes((Resolve-Path $Icon).Path)
# Imagen del .ico con el tamaño más cercano por encima (la de 256 px viene en PNG, que System.Drawing.Icon no lee).
function Get-IconFrame([int]$size) {
    $count = [BitConverter]::ToUInt16($icoBytes, 4)
    $frames = for ($i = 0; $i -lt $count; $i++) {
        $e = 6 + 16 * $i
        $w = if ($icoBytes[$e] -eq 0) { 256 } else { [int]$icoBytes[$e] }
        [pscustomobject]@{ Size = $w; Entry = $e; Length = [BitConverter]::ToUInt32($icoBytes, $e + 8); Offset = [BitConverter]::ToUInt32($icoBytes, $e + 12) }
    }
    $f = $frames | Where-Object Size -ge $size | Sort-Object Size | Select-Object -First 1
    if (-not $f) { $f = $frames | Sort-Object Size -Descending | Select-Object -First 1 }
    $data = New-Object byte[] $f.Length
    [Array]::Copy($icoBytes, $f.Offset, $data, 0, $f.Length)
    if ($data[0] -eq 0x89 -and $data[1] -eq 0x50) {
        return [System.Drawing.Bitmap]::new([System.Drawing.Image]::FromStream([IO.MemoryStream]::new($data)))
    }
    # Imagen BMP: se arma un .ico de una sola imagen para leerla con System.Drawing.Icon.
    $single = New-Object byte[] (22 + $data.Length)
    [Array]::Copy($icoBytes, 0, $single, 0, 6)
    $single[4] = 1; $single[5] = 0
    [Array]::Copy($icoBytes, $f.Entry, $single, 6, 12)
    [Array]::Copy([BitConverter]::GetBytes([uint32]22), 0, $single, 18, 4)
    [Array]::Copy($data, 0, $single, 22, $data.Length)
    $ico = New-Object System.Drawing.Icon([IO.MemoryStream]::new($single))
    $bmp = $ico.ToBitmap(); $ico.Dispose()
    return $bmp
}
function New-Logo([string]$name, [int]$w, [int]$h, [int]$size) {
    $src = Get-IconFrame $size
    $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($src, [int](($w - $size) / 2), [int](($h - $size) / 2), $size, $size)
    $bmp.Save((Join-Path $assets $name), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose(); $src.Dispose()
}
New-Logo "StoreLogo.png" 50 50 50
New-Logo "Square44x44Logo.png" 44 44 44
foreach ($s in 16, 24, 32, 48, 256) {
    New-Logo "Square44x44Logo.targetsize-$s.png" $s $s $s
    New-Logo "Square44x44Logo.targetsize-${s}_altform-unplated.png" $s $s $s
}
New-Logo "Square150x150Logo.png" 150 150 100
New-Logo "Wide310x150Logo.png" 310 150 100
New-Logo "Square310x310Logo.png" 310 310 200

$desc = "System monitor for the Windows taskbar: CPU, RAM, GPU, temperatures and network next to the tray."
$manifest = @"
<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
         xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
         xmlns:desktop="http://schemas.microsoft.com/appx/manifest/desktop/windows10"
         xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
         IgnorableNamespaces="uap desktop rescap">
  <Identity Name="$($esc::Escape($IdentityName))" Publisher="$($esc::Escape($Publisher))" Version="$Version.0" ProcessorArchitecture="x64" />
  <Properties>
    <DisplayName>WRAM</DisplayName>
    <PublisherDisplayName>$($esc::Escape($PublisherDisplayName))</PublisherDisplayName>
    <Logo>Assets\StoreLogo.png</Logo>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.19041.0" MaxVersionTested="10.0.26100.0" />
  </Dependencies>
  <Resources>
    <Resource Language="en-us" />
    <Resource Language="es-es" />
    <Resource Language="pt-br" />
    <Resource Language="fr-fr" />
    <Resource Language="ja-jp" />
    <Resource Language="zh-cn" />
  </Resources>
  <Applications>
    <Application Id="WRAM" Executable="WRAM.exe" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements DisplayName="WRAM" Description="$desc" BackgroundColor="transparent"
                          Square150x150Logo="Assets\Square150x150Logo.png" Square44x44Logo="Assets\Square44x44Logo.png">
        <uap:DefaultTile Wide310x150Logo="Assets\Wide310x150Logo.png" Square310x310Logo="Assets\Square310x310Logo.png" />
      </uap:VisualElements>
      <Extensions>
        <desktop:Extension Category="windows.startupTask" Executable="WRAM.exe" EntryPoint="Windows.FullTrustApplication">
          <desktop:StartupTask TaskId="WRAMStartup" Enabled="false" DisplayName="WRAM" />
        </desktop:Extension>
      </Extensions>
    </Application>
  </Applications>
  <Capabilities>
    <rescap:Capability Name="runFullTrust" />
  </Capabilities>
</Package>
"@
$utf8 = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $layout "AppxManifest.xml"), $manifest, $utf8)
if ($LayoutOnly) { (Resolve-Path $layout).Path; return }

# MakeAppx del Windows SDK más reciente instalado.
$kits = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
$makeappx = Get-ChildItem $kits -Directory | Where-Object Name -match '^10\.' | Sort-Object { [version]$_.Name } -Descending |
    ForEach-Object { Join-Path $_.FullName "x64\makeappx.exe" } | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $makeappx) { throw "No se encontró makeappx.exe (Windows SDK)" }
$out = Join-Path $OutDir "WRAM-$Version-x64.msix"
& $makeappx pack /o /d $layout /p $out | Out-Null
if ($LASTEXITCODE -ne 0) { throw "MakeAppx falló con código $LASTEXITCODE" }
(Resolve-Path $out).Path
