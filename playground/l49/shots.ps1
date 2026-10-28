$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..\..')
$repo = (Get-Location).Path
Write-Output "[l49] repo: $repo"

Get-ChildItem docs/screenshots -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
Write-Output ("[l49] docs shots after wipe: " + (Get-ChildItem docs/screenshots -Filter *.png -ErrorAction SilentlyContinue | Measure-Object).Count)

if (Test-Path artifacts/usage-user) {
    Remove-Item -Recurse -Force artifacts/usage-user
}
Write-Output "[l49] usage-user wiped"

$env:BT_REF_GROUND = 'ref_ground'
$env:BT_CONFIG = Join-Path $repo 'config/config.json'
$env:BT_ARTIFACTS = 'artifacts'
$exe = Join-Path $repo 'build/bin/bt-app.exe'
$shots = Join-Path $repo 'docs/screenshots'

$env:APPDATA = Join-Path $repo 'artifacts/usage-user'
Write-Output "[l49] usage step: $exe --usage $shots  APPDATA=$env:APPDATA BT_CONFIG=$env:BT_CONFIG BT_REF_GROUND=$env:BT_REF_GROUND BT_ARTIFACTS=$env:BT_ARTIFACTS"
& $exe --usage $shots
Write-Output "[l49] usage exit: $LASTEXITCODE"

$env:BT_CONFIG = Join-Path $repo 'artifacts/usage-absent-config.json'
$env:APPDATA = Join-Path $repo 'artifacts/usage-user'
Write-Output "[l49] fatal step: $exe --fatal-shot $shots  BT_CONFIG=$env:BT_CONFIG"
& $exe --fatal-shot $shots
Write-Output "[l49] fatal exit: $LASTEXITCODE"

Get-ChildItem docs/screenshots -Filter *.png | ForEach-Object { Write-Output ("[l49] shot: " + $_.Name + " " + $_.Length + " " + $_.LastWriteTime.ToString('HH:mm:ss')) }
exit 0
