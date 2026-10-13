$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $env:CARGO_TARGET_DIR) {
    throw 'CARGO_TARGET_DIR must point at a private coverage target dir outside the repo, run via make coverage'
}

$root = Split-Path -Parent $PSScriptRoot
$badgeDir = Join-Path $root 'docs/badges'
New-Item -ItemType Directory -Force $badgeDir | Out-Null

$json = cargo llvm-cov --workspace --summary-only --json | Out-String
if ($LASTEXITCODE -ne 0) { throw "cargo llvm-cov failed with exit $LASTEXITCODE" }
$report = $json | ConvertFrom-Json
$percent = [double]$report.data[0].totals.regions.percent

$value = '{0:F1}%' -f $percent
$label = 'coverage'
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

Set-Content -Path (Join-Path $badgeDir 'coverage.svg') -Value $svg -NoNewline
Write-Output ("coverage regions: {0:F1}%" -f $percent)
