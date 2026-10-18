$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'docs/diagrams'
$names = @(
    'workspace',
    'cube-core',
    'scheme',
    'data',
    'ga',
    'app-boot',
    'drill-flow',
    'memo-flow',
    'solve-flow',
    'verification-chain'
)

New-Item -ItemType Directory -Force $out | Out-Null
$sources = @()
foreach ($name in $names) {
    $source = Join-Path $out ($name + '.puml')
    if (-not (Test-Path $source)) { throw "missing diagram source $source" }
    $sources += $source
}
plantuml -tpng -charset UTF-8 @sources
if ($LASTEXITCODE -ne 0) { throw "plantuml render failed with exit $LASTEXITCODE" }
foreach ($name in $names) {
    $png = Join-Path $out ($name + '.png')
    if (-not (Test-Path $png)) { throw "missing rendered png $png" }
    Write-Output "rendered $name.png"
}
