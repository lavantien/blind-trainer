param([string]$Path = "artifacts/e2e/drill.png")

Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Path))

function Find-Region([int]$r, [int]$g, [int]$b, [int]$tol, [int]$y0, [int]$y1, [int]$x0 = 0, [int]$x1 = -1) {
    if ($x1 -lt 0) { $x1 = $bmp.Width }
    $minX = $bmp.Width; $maxX = -1; $minY = $bmp.Height; $maxY = -1; $count = 0
    for ($y = $y0; $y -lt $y1; $y++) {
        for ($x = $x0; $x -lt $x1; $x++) {
            $p = $bmp.GetPixel($x, $y)
            if ([math]::Abs($p.R - $r) -le $tol -and [math]::Abs($p.G - $g) -le $tol -and [math]::Abs($p.B - $b) -le $tol) {
                $count++
                if ($x -lt $minX) { $minX = $x }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }
    if ($maxX -lt 0) { return $null }
    [pscustomobject]@{ Count = $count; MinX = $minX; MaxX = $maxX; MinY = $minY; MaxY = $maxY; CenterX = [math]::Round(($minX + $maxX) / 2) }
}

$h = $bmp.Height
$third = [int]($h / 3)
$twoThirds = [int](2 * $h / 3)
$uWhite = Find-Region 242 242 242 3 150 375 100 400
$fGreen = Find-Region 51 191 89 10 $third $twoThirds
$bBlue = Find-Region 51 115 230 10 $third $twoThirds
$dYellow = Find-Region 242 217 38 10 $twoThirds $h

"U white   : $($uWhite | ConvertTo-Json -Compress)"
"F green   : $($fGreen | ConvertTo-Json -Compress)"
"B blue    : $($bBlue | ConvertTo-Json -Compress)"
"D yellow  : $($dYellow | ConvertTo-Json -Compress)"
if ($uWhite -and $fGreen) {
    "U center x = $($uWhite.CenterX), F center x = $($fGreen.CenterX), delta = $($uWhite.CenterX - $fGreen.CenterX)"
}
if ($bBlue) {
    "B right edge = $($bBlue.MaxX)"
}
$bmp.Dispose()
