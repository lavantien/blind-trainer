$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $env:CARGO_TARGET_DIR) {
    throw 'CARGO_TARGET_DIR must point at a private coverage target dir outside the repo, run via make coverage'
}

$root = Split-Path -Parent $PSScriptRoot
$badgeDir = Join-Path $root 'docs/badges'
New-Item -ItemType Directory -Force $badgeDir | Out-Null

$json = cargo llvm-cov --workspace --json | Out-String
if ($LASTEXITCODE -ne 0) { throw "cargo llvm-cov failed with exit $LASTEXITCODE" }
$report = $json | ConvertFrom-Json
$data = $report.data[0]
$totals = $data.totals
$percent = [double]$totals.regions.percent

$reportDir = Join-Path $root 'artifacts/coverage'
New-Item -ItemType Directory -Force $reportDir | Out-Null

$prefix = ($root -replace '\\', '/') + '/'
function Convert-Rel([string]$absolute) {
    $rel = $absolute -replace '\\', '/'
    if ($rel.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) { $rel.Substring($prefix.Length) } else { $rel }
}

$all = foreach ($file in $data.files) {
    $rel = Convert-Rel $file.filename
    $crate = if ($rel -match '^(crates|tools)/([^/]+)/') { $Matches[2] } else { $null }
    [pscustomobject]@{ path = $rel; crate = $crate; regions = $file.summary.regions }
}
$deadByFile = @{}
foreach ($fn in ($data.functions | Where-Object { [long]$_.count -eq 0 })) {
    $rel = Convert-Rel ([string]$fn.filenames[0])
    if (-not $deadByFile.ContainsKey($rel)) { $deadByFile[$rel] = [System.Collections.Generic.List[string]]::new() }
    $deadByFile[$rel].Add($fn.name)
}
$crates = $all | Where-Object { $_.crate } | Group-Object crate
$crateRows = foreach ($group in $crates) {
    $covered = ($group.Group | Measure-Object { [int]$_.regions.covered } -Sum).Sum
    $count = ($group.Group | Measure-Object { [int]$_.regions.count } -Sum).Sum
    [pscustomobject]@{ crate = $group.Name; covered = $covered; total = $count; pct = if ($count) { 100.0 * $covered / $count } else { 100.0 } }
}
$worst = $all |
    Where-Object { $_.path -match '^((crates|tools)/[^/]+/src|fuzz)/' -and [int]$_.regions.count -gt 0 } |
    Sort-Object { [double]$_.regions.percent } |
    Select-Object -First 25

$lines = @()
$lines += '# workspace coverage report'
$lines += ''
$lines += "generated: $((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))"
$lines += "regions: $([int]$totals.regions.covered) of $([int]$totals.regions.count) ($('{0:F1}' -f $percent)%)"
$lines += ''
$lines += '| crate | regions | percent |'
$lines += '| --- | --- | --- |'
foreach ($row in ($crateRows | Sort-Object pct)) {
    $lines += ('| {0} | {1}/{2} | {3:F1}% |' -f $row.crate, $row.covered, $row.total, $row.pct)
}
$lines += ''
$lines += 'worst 25 files:'
$lines += ''
foreach ($file in $worst) {
    $lines += ('- {0}: {1}/{2} regions ({3:F1}%)' -f $file.path, [int]$file.regions.covered, [int]$file.regions.count, [double]$file.regions.percent)
    if ($deadByFile.ContainsKey($file.path)) {
        $names = @($deadByFile[$file.path] | Select-Object -First 5 | ForEach-Object { if ($_.Length -gt 60) { $_.Substring(0, 60) } else { $_ } })
        $lines += ('  uncovered: {0}' -f ($names -join ', '))
    }
}
Set-Content -Path (Join-Path $reportDir 'report.md') -Value ($lines -join "`n")

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
