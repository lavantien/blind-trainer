$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$config = Get-Content (Join-Path $root 'config/config.json') -Raw | ConvertFrom-Json
$dataVendor = Join-Path $root $config.paths.data_vendor
$manifestPath = Join-Path $dataVendor 'manifest.json'
if (-not (Test-Path $manifestPath)) {
    throw "manifest missing: $manifestPath, run make gen-data first"
}
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$total = [int]$manifest.cases.entries
$covered = [int]$manifest.cases.edge.covered + [int]$manifest.cases.corner.covered
if ($total -le 0) {
    throw 'manifest reports no cases'
}
$percent = [math]::Min(100.0, [math]::Max(0.0, 100.0 * $covered / $total))

$badgeDir = Join-Path $root 'docs/badges'
New-Item -ItemType Directory -Force $badgeDir | Out-Null

$value = '{0:F1}%' -f $percent
$label = 'alg coverage'
if ($percent -ge 80) {
    $color = '#2ea043'
} elseif ($percent -ge 60) {
    $color = '#bf8700'
} else {
    $color = '#cf222e'
}
$labelW = [math]::Ceiling($label.Length * 6.6 + 18)
$valueW = [math]::Ceiling($value.Length * 6.6 + 18)
$w = $labelW + $valueW
$labelMid = [math]::Round($labelW / 2)
$valueMid = $labelW + [math]::Round($valueW / 2)

$svg = @"
<svg xmlns="http://www.w3.org/2000/svg" width="$w" height="20" role="img" aria-label="${label}: ${value}">
  <title>${label}: ${value}</title>
  <clipPath id="round"><rect width="$w" height="20" rx="3"/></clipPath>
  <g clip-path="url(#round)">
    <rect width="$labelW" height="20" fill="#57606a"/>
    <rect x="$labelW" width="$valueW" height="20" fill="$color"/>
  </g>
  <text x="$labelMid" y="14" fill="#ffffff" font-family="Verdana,Geneva,DejaVu Sans,sans-serif" font-size="11" text-anchor="middle">$label</text>
  <text x="$valueMid" y="14" fill="#ffffff" font-family="Verdana,Geneva,DejaVu Sans,sans-serif" font-size="11" text-anchor="middle">$value</text>
</svg>
"@

Set-Content -Path (Join-Path $badgeDir 'alg-coverage.svg') -Value $svg -NoNewline
Write-Output ("alg coverage: {0:F1}% ({1} of {2} cases)" -f $percent, $covered, $total)
