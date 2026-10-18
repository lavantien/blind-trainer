param(
    [string]$Shot = "artifacts/e2e/drill.png",
    [string]$Base = "tests/visual/baselines/drill.png",
    [int]$Tolerance = 48
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$source = @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class RgbaDiff {
    public static long[] Compare(string shotPath, string basePath, int tolerance) {
        using (Bitmap shot = new Bitmap(shotPath))
        using (Bitmap baseImg = new Bitmap(basePath)) {
            int w = Math.Min(shot.Width, baseImg.Width);
            int h = Math.Min(shot.Height, baseImg.Height);
            long pixelsOver = 0, maxDelta = 0;
            long minX = long.MaxValue, minY = long.MaxValue, maxX = -1, maxY = -1;
            long[] rowCounts = new long[h];
            long[] colCounts = new long[w];
            Rectangle rect = new Rectangle(0, 0, w, h);
            BitmapData shotData = shot.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            BitmapData baseData = baseImg.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            int[] shotPx = new int[w * h];
            int[] basePx = new int[w * h];
            Marshal.Copy(shotData.Scan0, shotPx, 0, shotPx.Length);
            Marshal.Copy(baseData.Scan0, basePx, 0, basePx.Length);
            shot.UnlockBits(shotData);
            baseImg.UnlockBits(baseData);
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int a = shotPx[y * w + x];
                    int b = basePx[y * w + x];
                    int da = Math.Abs(((a >> 24) & 255) - ((b >> 24) & 255));
                    int dr = Math.Abs(((a >> 16) & 255) - ((b >> 16) & 255));
                    int dg = Math.Abs(((a >> 8) & 255) - ((b >> 8) & 255));
                    int db = Math.Abs((a & 255) - (b & 255));
                    int delta = Math.Max(Math.Max(da, dr), Math.Max(dg, db));
                    if (delta > maxDelta) maxDelta = delta;
                    if (delta > tolerance) {
                        pixelsOver++;
                        rowCounts[y]++;
                        colCounts[x]++;
                        if (x < minX) minX = x;
                        if (y < minY) minY = y;
                        if (x > maxX) maxX = x;
                        if (y > maxY) maxY = y;
                    }
                }
            }
            long[] result = new long[7 + h + w];
            result[0] = pixelsOver;
            result[1] = maxDelta;
            result[2] = minX;
            result[3] = minY;
            result[4] = maxX;
            result[5] = maxY;
            result[6] = 0;
            for (int y = 0; y < h; y++) result[7 + y] = rowCounts[y];
            for (int x = 0; x < w; x++) result[7 + h + x] = colCounts[x];
            return result;
        }
    }
}
"@
$refs = [System.AppDomain]::CurrentDomain.GetAssemblies() | Where-Object { $_.Location } | ForEach-Object { $_.Location } | Select-Object -Unique
Add-Type -TypeDefinition $source -Language CSharp -ReferencedAssemblies $refs

$result = [RgbaDiff]::Compare((Resolve-Path $Shot).Path, (Resolve-Path $Base).Path, $Tolerance)
$head = $result[0..6]
Write-Output ("pixels_over={0} max_delta={1} bbox=({2},{3})..({4},{5})" -f $head[0], $head[1], $head[2], $head[3], $head[4], $head[5])
$dim = [System.Drawing.Image]::FromFile((Resolve-Path $Shot).Path)
Write-Output ("dims={0}x{1}" -f $dim.Width, $dim.Height)
$dim.Dispose()

$script:shotImg = [System.Drawing.Image]::FromFile((Resolve-Path $Shot).Path)
$script:baseImg = [System.Drawing.Image]::FromFile((Resolve-Path $Base).Path)
$h = [Math]::Min($script:shotImg.Height, $script:baseImg.Height)
$w = [Math]::Min($script:shotImg.Width, $script:baseImg.Width)

$rowCounts = $result[7..(6 + $h)]
$colCounts = $result[(7 + $h)..(6 + $h + $w)]
$rowsWith = @()
for ($y = 0; $y -lt $h; $y++) { if ($rowCounts[$y] -gt 0) { $rowsWith += [pscustomobject]@{ y = $y; count = $rowCounts[$y] } } }
Write-Output ("rows with over-pixels: {0}, span y {1}..{2}" -f $rowsWith.Count, ($rowsWith | Measure-Object -Property y -Minimum).Minimum, ($rowsWith | Measure-Object -Property y -Maximum).Maximum)
$bands = @{}
foreach ($r in $rowsWith) {
    $band = [Math]::Floor($r.y / 40) * 40
    if (-not $bands.ContainsKey($band)) { $bands[$band] = 0 }
    $bands[$band] += $r.count
}
Write-Output "y-band (40px) totals:"
foreach ($k in ($bands.Keys | Sort-Object)) { Write-Output ("  y {0}-{1}: {2} px" -f $k, ($k + 39), $bands[$k]) }
$colsWith = @()
for ($x = 0; $x -lt $w; $x++) { if ($colCounts[$x] -gt 0) { $colsWith += [pscustomobject]@{ x = $x; count = $colCounts[$x] } } }
$xbands = @{}
foreach ($c in $colsWith) {
    $band = [Math]::Floor($c.x / 80) * 80
    if (-not $xbands.ContainsKey($band)) { $xbands[$band] = 0 }
    $xbands[$band] += $c.count
}
Write-Output "x-band (80px) totals:"
foreach ($k in ($xbands.Keys | Sort-Object)) { Write-Output ("  x {0}-{1}: {2} px" -f $k, ($k + 79), $xbands[$k]) }
$script:shotImg.Dispose()
$script:baseImg.Dispose()
