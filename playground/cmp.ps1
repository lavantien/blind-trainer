$ErrorActionPreference = 'Stop'
function Get-StaleScratches([string]$temp, [int]$staleHours) {
    $cutoff = (Get-Date).AddHours(-$staleHours)
    @(Get-ChildItem -LiteralPath $temp -Directory -Filter 'cargo-mutants*' -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -lt $cutoff } |
        ForEach-Object { $_.FullName })
}
$WorkDir = 'artifacts/clean-rust-selftest'
if (Test-Path -LiteralPath $WorkDir) { Remove-Item -LiteralPath $WorkDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
$work = (Resolve-Path -LiteralPath $WorkDir).Path
$repoFake = Join-Path $work 'repo'
$inc = Join-Path $repoFake 'target/debug/incremental'
New-Item -ItemType Directory -Force -Path $inc | Out-Null
Set-Content -Path (Join-Path $inc 'cache.bin') -Value 'x'
$tempFake = Join-Path $work 'temp'
New-Item -ItemType Directory -Force -Path $tempFake | Out-Null
$staleDir = Join-Path $tempFake 'cargo-mutants-stale'
$freshDir = Join-Path $tempFake 'cargo-mutants-fresh'
New-Item -ItemType Directory -Force -Path $staleDir, $freshDir | Out-Null
Set-Content -Path (Join-Path $staleDir 'a') -Value 'x'
Set-Content -Path (Join-Path $freshDir 'a') -Value 'x'
(Get-Item -LiteralPath $staleDir).LastWriteTime = (Get-Date).AddHours(-8)
(Get-Item -LiteralPath $freshDir).LastWriteTime = (Get-Date).AddHours(-1)
$scratches = Get-StaleScratches $tempFake 6
"type=$($scratches.GetType().FullName) count=$($scratches.Count)"
"staleDir=[$staleDir]"
"picked0=[$($scratches[0])]"
"clause_count=$($scratches.Count -ne 1)"
"clause_ne=$($scratches[0] -ne $staleDir)"
