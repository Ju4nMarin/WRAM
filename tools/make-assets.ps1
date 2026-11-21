# Genera los recursos de marca a partir de las imágenes maestras del logo:
#   assets/logo-mark.png          W blanca con transparencia (512 px)
#   assets/logo-horizontal-light.png  logo horizontal blanco con transparencia (para fondos oscuros)
#   assets/logo-horizontal-dark.png   logo horizontal oscuro con transparencia (para fondos claros)
#   res/wram.ico                  ícono de la aplicación (16 a 256 px)
#
# Uso: powershell -ExecutionPolicy Bypass -File tools/make-assets.ps1 -Mark <logo blanco sobre oscuro> -Horizontal <horizontal blanco sobre oscuro>
param(
    [Parameter(Mandatory = $true)][string]$Mark,
    [Parameter(Mandatory = $true)][string]$Horizontal
)
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$assets = Join-Path $root 'assets'
New-Item -ItemType Directory -Force $assets | Out-Null

# Convierte una imagen de trazo claro sobre fondo oscuro en una máscara: el alfa sale del brillo.
function Get-Mask([string]$path) {
    $src = New-Object System.Drawing.Bitmap $path
    $bg = $src.GetPixel(2, 2).GetBrightness()
    $w = $src.Width; $h = $src.Height
    $alpha = New-Object 'byte[,]' $w, $h
    $minX = $w; $minY = $h; $maxX = 0; $maxY = 0
    for ($y = 0; $y -lt $h; $y++) {
        for ($x = 0; $x -lt $w; $x++) {
            $l = $src.GetPixel($x, $y).GetBrightness()
            $a = [Math]::Round([Math]::Max(0, [Math]::Min(1, ($l - $bg - 0.04) / (0.96 - $bg))) * 255)
            $alpha[$x, $y] = [byte]$a
            if ($a -gt 24) {
                if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
                if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }
    $src.Dispose()
    return @{ Alpha = $alpha; X = $minX; Y = $minY; W = $maxX - $minX + 1; H = $maxY - $minY + 1 }
}

function Save-Tinted($mask, [System.Drawing.Color]$color, [string]$out, [int]$targetWidth, [int]$pad) {
    $full = New-Object System.Drawing.Bitmap ($mask.W + 2 * $pad), ($mask.H + 2 * $pad), ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($y = 0; $y -lt $mask.H; $y++) {
        for ($x = 0; $x -lt $mask.W; $x++) {
            $a = $mask.Alpha[($x + $mask.X), ($y + $mask.Y)]
            if ($a -gt 0) { $full.SetPixel($x + $pad, $y + $pad, [System.Drawing.Color]::FromArgb($a, $color.R, $color.G, $color.B)) }
        }
    }
    $scale = $targetWidth / $full.Width
    $res = New-Object System.Drawing.Bitmap $targetWidth, ([int]($full.Height * $scale)), ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($res)
    $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'
    $g.DrawImage($full, 0, 0, $res.Width, $res.Height)
    $res.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $full.Dispose()
    return $res
}

$white = [System.Drawing.Color]::White
$ink = [System.Drawing.Color]::FromArgb(255, 0x1B, 0x1E, 0x22)

$markMask = Get-Mask $Mark
$markPng = Save-Tinted $markMask $white (Join-Path $assets 'logo-mark.png') 512 0
$horMask = Get-Mask $Horizontal
(Save-Tinted $horMask $white (Join-Path $assets 'logo-horizontal-light.png') 640 0).Dispose()
(Save-Tinted $horMask $ink (Join-Path $assets 'logo-horizontal-dark.png') 640 0).Dispose()

# Ícono: W blanca sobre cuadrado redondeado oscuro.
$sizes = 16, 20, 24, 32, 40, 48, 64, 256
$pngs = @()
foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'; $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'
    $r = [Math]::Max(3, [int]($s * 0.22)); $d = 2 * $r; $w = $s - 1
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddArc(0, 0, $d, $d, 180, 90); $path.AddArc($w - $d, 0, $d, $d, 270, 90)
    $path.AddArc($w - $d, $w - $d, $d, $d, 0, 90); $path.AddArc(0, $w - $d, $d, $d, 90, 90); $path.CloseFigure()
    $g.FillPath((New-Object System.Drawing.SolidBrush $ink), $path)
    $mw = $s * 0.72; $mh = $mw * $markPng.Height / $markPng.Width
    $g.DrawImage($markPng, [single](($s - $mw) / 2), [single](($s - $mh) / 2), [single]$mw, [single]$mh)
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += , $ms.ToArray()
    $g.Dispose(); $bmp.Dispose()
}
$markPng.Dispose()

$ico = Join-Path $root 'res\wram.ico'
$fs = [System.IO.File]::Create($ico)
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([UInt16]0); $bw.Write([UInt16]1); $bw.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $dim = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
    $bw.Write([Byte]$dim); $bw.Write([Byte]$dim); $bw.Write([Byte]0); $bw.Write([Byte]0)
    $bw.Write([UInt16]1); $bw.Write([UInt16]32)
    $bw.Write([UInt32]$pngs[$i].Length); $bw.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $bw.Write($p) }
$bw.Close()
Get-ChildItem $assets, $ico | Select-Object Name, Length

# Versiones en base64 que el build incrusta en la interfaz (tools/minify-html.cmake).
foreach ($n in 'logo-horizontal-light', 'logo-mark') {
    $b64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $assets "$n.png")))
    [IO.File]::WriteAllText((Join-Path $assets "$n.b64"), $b64)
}
