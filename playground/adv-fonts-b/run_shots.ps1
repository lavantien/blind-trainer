$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..\..')
$repo = (Get-Location).Path
$scratch = Join-Path $repo 'playground/adv-fonts-b'
$shots = Join-Path $scratch 'shots'
$usageUser = Join-Path $scratch 'usage-user'
Write-Output "[adv-fonts-b] repo: $repo"

if (Test-Path $shots) { Remove-Item -Recurse -Force $shots }
New-Item -ItemType Directory -Force -Path $shots | Out-Null
if (Test-Path $usageUser) { Remove-Item -Recurse -Force $usageUser }

$env:BT_REF_GROUND = 'ref_ground'
$env:BT_CONFIG = Join-Path $repo 'config/config.json'
$env:BT_ARTIFACTS = 'artifacts'
$exe = Join-Path $repo 'build/bin/bt-app.exe'

$env:APPDATA = $usageUser
Write-Output "[adv-fonts-b] usage step APPDATA=$env:APPDATA BT_CONFIG=$env:BT_CONFIG"
& $exe --usage $shots
$usageExit = $LASTEXITCODE
Write-Output "[adv-fonts-b] usage exit: $usageExit"

$env:BT_CONFIG = Join-Path $repo 'artifacts/usage-absent-config.json'
& $exe --fatal-shot $shots
$fatalExit = $LASTEXITCODE
Write-Output "[adv-fonts-b] fatal exit: $fatalExit"

$docs = Join-Path $repo 'docs/screenshots'
foreach ($png in Get-ChildItem $shots -Filter *.png) {
    $doc = Join-Path $docs $png.Name
    $same = -1
    if (Test-Path $doc) {
        $a = (Get-FileHash $png.FullName -Algorithm SHA256).Hash
        $b = (Get-FileHash $doc -Algorithm SHA256).Hash
        $same = [int]($a -eq $b)
    }
    Write-Output ("[adv-fonts-b] shot: " + $png.Name + " bytes=" + $png.Length + " matches_docs=" + $same)
}
exit 0
