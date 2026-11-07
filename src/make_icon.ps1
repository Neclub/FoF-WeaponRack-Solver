param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Dest
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

# Sizes Windows 11 asks for across the title bar, taskbar, Alt-Tab, and Explorer,
# including the 125%, 150%, and 200% scales of those targets.
$sizes = @(16, 20, 24, 32, 40, 48, 64, 96, 128, 256)

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class WrcIcon {
    public static Bitmap CropSquare(string path) {
        using (var raw = new Bitmap(path)) {
            var src = new Bitmap(raw.Width, raw.Height, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(src)) {
                g.DrawImage(raw, 0, 0, raw.Width, raw.Height);
            }
            var data = src.LockBits(new Rectangle(0, 0, src.Width, src.Height),
                ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            int minX = src.Width, minY = src.Height, maxX = -1, maxY = -1;
            var bytes = new byte[Math.Abs(data.Stride) * src.Height];
            Marshal.Copy(data.Scan0, bytes, 0, bytes.Length);
            for (int y = 0; y < src.Height; y++) {
                int row = y * Math.Abs(data.Stride);
                for (int x = 0; x < src.Width; x++) {
                    if (bytes[row + x * 4 + 3] > 24) {
                        if (x < minX) minX = x;
                        if (y < minY) minY = y;
                        if (x > maxX) maxX = x;
                        if (y > maxY) maxY = y;
                    }
                }
            }
            src.UnlockBits(data);
            if (maxX < minX) {
                src.Dispose();
                throw new InvalidOperationException("Icon image has no visible pixels.");
            }
            int bw = maxX - minX + 1;
            int bh = maxY - minY + 1;
            int pad = Math.Max(2, (int)Math.Round(Math.Max(bw, bh) * 0.04));
            minX = Math.Max(0, minX - pad);
            minY = Math.Max(0, minY - pad);
            maxX = Math.Min(src.Width - 1, maxX + pad);
            maxY = Math.Min(src.Height - 1, maxY + pad);
            bw = maxX - minX + 1;
            bh = maxY - minY + 1;
            int side = Math.Max(bw, bh);
            int left = minX - (side - bw) / 2;
            int top = minY - (side - bh) / 2;
            if (left < 0) left = 0;
            if (top < 0) top = 0;
            if (left + side > src.Width) left = Math.Max(0, src.Width - side);
            if (top + side > src.Height) top = Math.Max(0, src.Height - side);
            side = Math.Min(side, Math.Min(src.Width - left, src.Height - top));
            var square = new Bitmap(side, side, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(square)) {
                g.Clear(Color.Transparent);
                g.CompositingMode = CompositingMode.SourceCopy;
                g.InterpolationMode = InterpolationMode.NearestNeighbor;
                g.DrawImage(src, new Rectangle(0, 0, side, side),
                    new Rectangle(left, top, side, side), GraphicsUnit.Pixel);
            }
            src.Dispose();
            return square;
        }
    }

    public static Bitmap Scale(Bitmap src, int size) {
        Bitmap current = src;
        int w = src.Width;
        bool owned = false;
        while (w > size * 2) {
            int nextW = Math.Max(size, w / 2);
            var step = new Bitmap(nextW, nextW, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(step)) {
                g.Clear(Color.Transparent);
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                g.CompositingQuality = CompositingQuality.HighQuality;
                g.CompositingMode = CompositingMode.SourceOver;
                g.DrawImage(current, new Rectangle(0, 0, nextW, nextW));
            }
            if (owned) current.Dispose();
            current = step;
            owned = true;
            w = nextW;
        }
        if (w == size && owned) return current;
        var final = new Bitmap(size, size, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(final)) {
            g.Clear(Color.Transparent);
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.CompositingQuality = CompositingQuality.HighQuality;
            g.CompositingMode = CompositingMode.SourceOver;
            g.DrawImage(current, new Rectangle(0, 0, size, size));
        }
        if (owned) current.Dispose();
        return final;
    }
}
"@

if (-not (Test-Path -LiteralPath $Source)) {
    throw "Missing icon image: $Source"
}

$destPath = $Dest
if (-not [System.IO.Path]::IsPathRooted($destPath)) {
    $destPath = Join-Path (Get-Location) $destPath
}
$folder = Split-Path -Parent $destPath
if (-not (Test-Path -LiteralPath $folder)) { New-Item -ItemType Directory -Path $folder | Out-Null }

$square = [WrcIcon]::CropSquare((Resolve-Path -LiteralPath $Source).Path)
try {
    $pngs = New-Object System.Collections.Generic.List[byte[]]
    foreach ($size in $sizes) {
        $frame = [WrcIcon]::Scale($square, $size)
        try {
            $ms = New-Object System.IO.MemoryStream
            $frame.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
            $pngs.Add($ms.ToArray())
            $ms.Dispose()
        } finally {
            $frame.Dispose()
        }
    }
} finally {
    $square.Dispose()
}

$fs = [System.IO.File]::Create($destPath)
try {
    $bw = New-Object System.IO.BinaryWriter($fs)
    $bw.Write([uint16]0)
    $bw.Write([uint16]1)
    $bw.Write([uint16]$pngs.Count)
    $offset = 6 + (16 * $pngs.Count)
    for ($i = 0; $i -lt $pngs.Count; $i++) {
        $dim = [byte]$(if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] })
        $bw.Write($dim)
        $bw.Write($dim)
        $bw.Write([byte]0)
        $bw.Write([byte]0)
        $bw.Write([uint16]1)
        $bw.Write([uint16]32)
        $bw.Write([uint32]$pngs[$i].Length)
        $bw.Write([uint32]$offset)
        $offset += $pngs[$i].Length
    }
    foreach ($png in $pngs) { $bw.Write($png) }
    $bw.Flush()
} finally {
    $fs.Dispose()
}

Write-Output ("icon=" + $destPath + " sizes=" + ($sizes -join ","))
