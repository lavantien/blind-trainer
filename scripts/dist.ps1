param([Parameter(Mandatory)][string]$Version)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$root = Join-Path $repo "artifacts/dist"
$staging = Join-Path $root "blind-trainer"
$zip = Join-Path $root "blind-trainer-$Version-windows-x64.zip"
if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
New-Item -ItemType Directory (Join-Path $staging 'assets/fonts') -Force | Out-Null
New-Item -ItemType Directory (Join-Path $staging 'config') -Force | Out-Null
Copy-Item (Join-Path $repo 'target/release/blind-trainer.exe') $staging
Copy-Item (Join-Path $repo 'assets/fonts/*') (Join-Path $staging 'assets/fonts')
Copy-Item (Join-Path $repo 'config/config.json') (Join-Path $staging 'config')
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $staging -DestinationPath $zip
Get-Item $zip | Select-Object Name, Length
