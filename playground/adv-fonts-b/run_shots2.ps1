$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..\..')
$repo = (Get-Location).Path
$shots = Join-Path $repo 'playground/adv-fonts-b/shots2'
$usageUser = Join-Path $repo 'playground/adv-fonts-b/usage-user2'
if (Test-Path $shots) { Remove-Item -Recurse -Force $shots }
New-Item -ItemType Directory -Force -Path $shots | Out-Null
if (Test-Path $usageUser) { Remove-Item -Recurse -Force $usageUser }
$env:BT_REF_GROUND = 'ref_ground'
$env:BT_CONFIG = Join-Path $repo 'config/config.json'
$env:BT_ARTIFACTS = 'artifacts'
$env:APPDATA = $usageUser
& (Join-Path $repo 'build/bin/bt-app.exe') --usage $shots
Write-Output "[adv-fonts-b] usage2 exit: $LASTEXITCODE"
$first = Join-Path $repo 'playground/adv-fonts-b/shots'
foreach ($png in Get-ChildItem $shots -Filter *.png) {
    $a = (Get-FileHash $png.FullName -Algorithm SHA256).Hash
    $b = (Get-FileHash (Join-Path $first $png.Name) -Algorithm SHA256).Hash
    Write-Output ("[adv-fonts-b] run2 " + $png.Name + " same_as_run1=" + [int]($a -eq $b))
}
exit 0
