# Rebuild Win32/app_icon.ico from Asset/Robot_High.png
# Uses classic BMP/DIB ICO frames (not PNG-in-ICO) so MSVC rc.exe embeds correctly in Explorer.
$ErrorActionPreference = 'Stop'
# MSBuild sets LIB/LIBPATH in ways that break Add-Type's C# compiler; clear before compiling inline C#.
$env:LIB = ''
$env:LIBPATH = ''
$root = Split-Path -Parent $PSScriptRoot
$src = Join-Path $root 'Asset\Robot_High.png'
$dst = Join-Path $PSScriptRoot 'app_icon.ico'

Add-Type -AssemblyName System.Drawing

$icoWriterSrc = @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

public static class ClassicIcoWriter
{
    static void WriteLe16(Stream s, ushort v)
    {
        s.WriteByte((byte)(v & 0xFF));
        s.WriteByte((byte)(v >> 8));
    }

    static void WriteLe32(Stream s, uint v)
    {
        s.WriteByte((byte)(v & 0xFF));
        s.WriteByte((byte)((v >> 8) & 0xFF));
        s.WriteByte((byte)((v >> 16) & 0xFF));
        s.WriteByte((byte)((v >> 24) & 0xFF));
    }

    static void WriteLe16A(byte[] a, int i, ushort v)
    {
        a[i] = (byte)v;
        a[i + 1] = (byte)(v >> 8);
    }

    static void WriteLe32A(byte[] a, int i, uint v)
    {
        a[i] = (byte)v;
        a[i + 1] = (byte)(v >> 8);
        a[i + 2] = (byte)(v >> 16);
        a[i + 3] = (byte)(v >> 24);
    }

    public static void Save(string path, Bitmap[] frames)
    {
        var dibList = new List<byte[]>();
        foreach (var bmp in frames)
            dibList.Add(CreateDibImage(bmp));

        int count = dibList.Count;
        int headerSize = 6 + 16 * count;
        int offset = headerSize;

        using (var fs = new FileStream(path, FileMode.Create, FileAccess.Write))
        {
            WriteLe16(fs, 0);
            WriteLe16(fs, 1);
            WriteLe16(fs, (ushort)count);

            for (int i = 0; i < count; i++)
            {
                var dib = dibList[i];
                int w = frames[i].Width;
                int h = frames[i].Height;
                fs.WriteByte((byte)(w >= 256 ? 0 : w));
                fs.WriteByte((byte)(h >= 256 ? 0 : h));
                fs.WriteByte(0);
                fs.WriteByte(0);
                WriteLe16(fs, 1);
                WriteLe16(fs, 32);
                WriteLe32(fs, (uint)dib.Length);
                WriteLe32(fs, (uint)offset);
                offset += dib.Length;
            }

            foreach (var dib in dibList)
                fs.Write(dib, 0, dib.Length);
        }
    }

    static byte[] CreateDibImage(Bitmap bmp)
    {
        int w = bmp.Width;
        int h = bmp.Height;
        int xorStride = w * 4;
        int xorSize = xorStride * h;
        int andRowBytes = ((w + 31) / 32) * 4;
        int andSize = andRowBytes * h;
        var dib = new byte[40 + xorSize + andSize];

        WriteLe32A(dib, 0, 40);
        WriteLe32A(dib, 4, (uint)w);
        WriteLe32A(dib, 8, (uint)(h * 2));
        WriteLe16A(dib, 12, 1);
        WriteLe16A(dib, 14, 32);
        WriteLe32A(dib, 16, 0);
        WriteLe32A(dib, 20, (uint)(xorSize + andSize));
        WriteLe32A(dib, 24, 0);
        WriteLe32A(dib, 28, 0);
        WriteLe32A(dib, 32, 0);
        WriteLe32A(dib, 36, 0);

        var rect = new Rectangle(0, 0, w, h);
        BitmapData data = bmp.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        try
        {
            for (int y = 0; y < h; y++)
            {
                int srcY = h - 1 - y;
                IntPtr pRow = IntPtr.Add(data.Scan0, srcY * data.Stride);
                Marshal.Copy(pRow, dib, 40 + y * xorStride, xorStride);
            }
        }
        finally
        {
            bmp.UnlockBits(data);
        }

        Array.Clear(dib, 40 + xorSize, andSize);
        return dib;
    }
}
'@

try {
    Add-Type -TypeDefinition $icoWriterSrc -ReferencedAssemblies System.Drawing -ErrorAction Stop
} catch {
    if ($_.Exception.Message -notmatch 'already exists') { throw }
}

function New-SquareFrame {
    param([System.Drawing.Image]$Source, [int]$Px)
    $bmp = New-Object System.Drawing.Bitmap $Px, $Px, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    # 32bpp ARGB DIB: letterbox is transparent; Robot_High.png alpha is preserved.
    $g.Clear([System.Drawing.Color]::Transparent)
    $iw = [float]$Source.Width
    $ih = [float]$Source.Height
    $scale = [Math]::Min($Px / $iw, $Px / $ih)
    $dw = [int][Math]::Round($iw * $scale)
    $dh = [int][Math]::Round($ih * $scale)
    $ox = [int](($Px - $dw) / 2)
    $oy = [int](($Px - $dh) / 2)
    $g.DrawImage($Source, $ox, $oy, $dw, $dh)
    $g.Dispose()
    return $bmp
}

$img = [System.Drawing.Image]::FromFile($src)
$bitmaps = New-Object System.Collections.Generic.List[System.Drawing.Bitmap]
try {
    $sizes = @(16, 32, 48, 64, 128, 256)
    foreach ($s in $sizes) {
        [void]$bitmaps.Add((New-SquareFrame -Source $img -Px $s))
    }
    [ClassicIcoWriter]::Save($dst, $bitmaps.ToArray())
}
finally {
    foreach ($b in $bitmaps) { $b.Dispose() }
    $img.Dispose()
}

Write-Host "Updated $dst (transparent letterbox, 32bpp alpha DIB ICO)"
