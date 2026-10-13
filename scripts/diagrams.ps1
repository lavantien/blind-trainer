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
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("bt-diagrams-" + $PID)

New-Item -ItemType Directory -Force $out | Out-Null
New-Item -ItemType Directory -Force $tmp | Out-Null

try {
    typst compile --format png --ppi 200 (Join-Path $root 'scripts/diagrams.typ') (Join-Path $tmp 'page-{n}.png')
    if ($LASTEXITCODE -ne 0) { throw "typst compile failed with exit $LASTEXITCODE" }
    for ($i = 0; $i -lt $names.Count; $i++) {
        $src = Join-Path $tmp ('page-{0:D2}' -f ($i + 1) + '.png')
        if (-not (Test-Path $src)) { throw "missing rendered page $($i + 1)" }
        Copy-Item $src (Join-Path $out ($names[$i] + '.png')) -Force
    }
} finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
