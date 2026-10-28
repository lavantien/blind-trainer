$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..\..')
$repo = (Get-Location).Path
$scratch = Join-Path $repo 'playground/adv-fonts-b'
$caps = @((Join-Path $scratch 'e2e-a'), (Join-Path $scratch 'e2e-b'))
$user = Join-Path $scratch 'e2e-user'
foreach ($c in $caps) {
    if (Test-Path $c) { Remove-Item -Recurse -Force $c }
    New-Item -ItemType Directory -Force -Path $c | Out-Null
}
if (Test-Path $user) { Remove-Item -Recurse -Force $user }

$env:BT_REF_GROUND = 'ref_ground'
$env:BT_CONFIG = Join-Path $repo 'config/config.json'
$env:BT_ARTIFACTS = 'artifacts'
$env:APPDATA = $user
$driver = Join-Path $repo 'build/bin/bt-e2e.exe'
$app = Join-Path $repo 'build/bin/bt-app.exe'

foreach ($c in $caps) {
    & $driver $app $c
    Write-Output ("[adv-fonts-b] capture " + $c + " exit: " + $LASTEXITCODE)
}

$hashA = @{}
foreach ($f in Get-ChildItem $caps[0] -Filter *.png) {
    $hashA[$f.Name] = (Get-FileHash $f.FullName -Algorithm SHA256).Hash
}
$hashB = @{}
foreach ($f in Get-ChildItem $caps[1] -Filter *.png) {
    $hashB[$f.Name] = (Get-FileHash $f.FullName -Algorithm SHA256).Hash
}
$drift = 0
foreach ($k in ($hashA.Keys + $hashB.Keys | Sort-Object -Unique)) {
    if (-not $hashA.ContainsKey($k) -or -not $hashB.ContainsKey($k)) {
        Write-Output "[adv-fonts-b] drift: $k missing in one capture"
        $drift++
        continue
    }
    $same = $hashA[$k] -eq $hashB[$k]
    if (-not $same) { $drift++ }
    Write-Output ("[adv-fonts-b] e2e " + $k + " cap1_eq_cap2=" + [int]$same)
}
$bases = Join-Path $repo 'tests/visual/baselines'
foreach ($f in Get-ChildItem $caps[1] -Filter *.png | Sort-Object Name) {
    $baseHash = (Get-FileHash (Join-Path $bases $f.Name) -Algorithm SHA256).Hash
    $same = $baseHash -eq $hashB[$f.Name]
    if (-not $same) { $drift++ }
    Write-Output ("[adv-fonts-b] e2e " + $f.Name + " cap2_eq_baseline=" + [int]$same)
}
Write-Output "[adv-fonts-b] determinism drift count: $drift"
exit 0
