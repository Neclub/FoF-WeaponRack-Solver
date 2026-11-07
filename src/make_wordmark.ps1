param(
    [string]$Dest = (Join-Path (Split-Path $PSScriptRoot -Parent) "Icon\title-wordmark.png"),
    [string]$Preview = (Join-Path (Split-Path $PSScriptRoot -Parent) "Icon\title-wordmark-preview.png")
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

# 1x layout matches the header row: 18pt-scale type plus a 3-color mark,
# rasterized at 16x so it stays sharp when drawn around 30px tall (and at 300% DPI).
$scale = 16
$em = 22.0 * $scale
$barH = [single](2.5 * $scale)
$barGap = [single](4.0 * $scale)
$wordGapExtra = 0.15
$pad = [single](2 * $scale)

$ink = [System.Drawing.Color]::FromArgb(255, 232, 236, 245)
$barColors = @(
    [System.Drawing.Color]::FromArgb(255, 79, 140, 255),
    [System.Drawing.Color]::FromArgb(255, 62, 207, 142),
    [System.Drawing.Color]::FromArgb(255, 255, 122, 69)
)
$words = @("Weapon", "Rack", "Solver")

$font = $null
foreach ($name in @("Segoe UI Semibold", "Segoe UI")) {
    $candidate = New-Object System.Drawing.Font($name, $em, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
    if ($candidate.Name -eq $name -or $name -eq "Segoe UI") {
        $font = $candidate
        break
    }
    $candidate.Dispose()
}
if (-not $font) { throw "Segoe UI is not installed." }

$style = [System.Drawing.FontStyle]::Regular
$family = $font.FontFamily
$units = [double]$family.GetEmHeight($style)
$baselineShift = $em * $family.GetCellAscent($style) / $units
$descent = $em * $family.GetCellDescent($style) / $units

$measureBmp = New-Object System.Drawing.Bitmap 8, 8
$measureG = [System.Drawing.Graphics]::FromImage($measureBmp)
$measureG.PageUnit = [System.Drawing.GraphicsUnit]::Pixel
$measureG.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
$sf = [System.Drawing.StringFormat]::GenericTypographic.Clone()
$sf.FormatFlags = [System.Drawing.StringFormatFlags]::NoWrap -bor [System.Drawing.StringFormatFlags]::MeasureTrailingSpaces

$space = $measureG.MeasureString(" ", $font, (New-Object System.Drawing.PointF 0, 0), $sf).Width
$wordGap = $space * (1.0 + $wordGapExtra)

$x = 0.0
$placed = @()
foreach ($i in 0..($words.Count - 1)) {
    $advance = $measureG.MeasureString($words[$i], $font, (New-Object System.Drawing.PointF 0, 0), $sf).Width
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $origin = New-Object System.Drawing.PointF $x, 0
    $path.AddString($words[$i], $family, [int]$style, $em, $origin, $sf)
    $bounds = $path.GetBounds()
    $placed += [pscustomobject]@{
        Path    = $path
        Advance = $advance
        X       = $x
        Ink     = $bounds
    }
    $x += $advance
    if ($i -lt $words.Count - 1) { $x += $wordGap }
}

$minX = [single]($placed | ForEach-Object { $_.Ink.Left } | Measure-Object -Minimum).Minimum
$maxX = [single]($placed | ForEach-Object { $_.Ink.Right } | Measure-Object -Maximum).Maximum
$minY = [single]($placed | ForEach-Object { $_.Ink.Top } | Measure-Object -Minimum).Minimum
$maxY = [single]($placed | ForEach-Object { $_.Ink.Bottom } | Measure-Object -Maximum).Maximum

$shortest = [single]($placed | ForEach-Object { $_.Ink.Width } | Measure-Object -Minimum).Minimum
$barW = [single]([Math]::Max($barH * 6, $shortest * 0.72))
$barTop = $maxY + $barGap

$shiftX = $pad - $minX
$shiftY = $pad - $minY
$width = [int][Math]::Ceiling(($maxX - $minX) + $pad * 2)
$height = [int][Math]::Ceiling(($barTop + $barH - $minY) + $pad * 2)

$bmp = New-Object System.Drawing.Bitmap $width, $height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::Transparent)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
$g.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias

$g.TranslateTransform($shiftX, $shiftY)
$textBrush = New-Object System.Drawing.SolidBrush $ink
foreach ($item in $placed) {
    $g.FillPath($textBrush, $item.Path)
}

for ($i = 0; $i -lt $placed.Count; $i++) {
    $inkBox = $placed[$i].Ink
    $bx = $inkBox.Left + ($inkBox.Width - $barW) / 2
    $by = $barTop
    $radius = $barH / 2
    $bar = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $radius * 2
    $bar.AddArc($bx, $by, $d, $d, 90, 180)
    $bar.AddArc($bx + $barW - $d, $by, $d, $d, 270, 180)
    $bar.CloseFigure()
    $brush = New-Object System.Drawing.SolidBrush $barColors[$i]
    $g.FillPath($brush, $bar)
    $brush.Dispose()
    $bar.Dispose()
}

$g.ResetTransform()
$dir = Split-Path $Dest -Parent
if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
$bmp.Save($Dest, [System.Drawing.Imaging.ImageFormat]::Png)

# Preview on the app background: large enough to judge, plus the real header size.
$bg = [System.Drawing.Color]::FromArgb(255, 18, 20, 26)
$showH = 168
$realH = 30
$margin = 56
$gap = 48
$showW = [int][Math]::Round($width * ($showH / [double]$height))
$realW = [int][Math]::Round($width * ($realH / [double]$height))
$cardW = [Math]::Max($showW, 720) + $margin * 2
$cardH = $margin + $showH + $gap + $realH + $margin
$card = New-Object System.Drawing.Bitmap $cardW, $cardH, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$cg = [System.Drawing.Graphics]::FromImage($card)
$cg.Clear($bg)
$cg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$cg.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$cg.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
$cg.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$cg.DrawImage($bmp, $margin, $margin, $showW, $showH)
$cg.DrawImage($bmp, $margin, $margin + $showH + $gap, $realW, $realH)
$card.Save($Preview, [System.Drawing.Imaging.ImageFormat]::Png)

Write-Output ("master {0}x{1} font={2} em={3}" -f $bmp.Width, $bmp.Height, $font.Name, $em)
Write-Output ("preview {0}" -f $Preview)
Write-Output ("dest {0}" -f $Dest)

$cg.Dispose()
$card.Dispose()
$textBrush.Dispose()
$g.Dispose()
$bmp.Dispose()
$sf.Dispose()
$measureG.Dispose()
$measureBmp.Dispose()
foreach ($item in $placed) { $item.Path.Dispose() }
$font.Dispose()
