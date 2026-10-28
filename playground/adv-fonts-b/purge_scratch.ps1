$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..\..')
foreach ($d in @(
    'playground/adv-fonts-b/shots',
    'playground/adv-fonts-b/shots2',
    'playground/adv-fonts-b/usage-user',
    'playground/adv-fonts-b/usage-user2',
    'playground/adv-fonts-b/e2e-a',
    'playground/adv-fonts-b/e2e-b',
    'playground/adv-fonts-b/e2e-user',
    'playground/adv-fonts-b/old'
)) {
    if (Test-Path $d) { Remove-Item -Recurse -Force $d }
}
Get-ChildItem 'playground/adv-fonts-b' -Recurse -File | ForEach-Object {
    Write-Output ($_.FullName.Replace((Get-Location).Path + '\', '') + ' ' + $_.Length)
}
