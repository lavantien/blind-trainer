$ErrorActionPreference = "Stop"
$repo = "C:\Users\lavantien\dev\github\blind-trainer"
$scratch = Join-Path $env:TEMP "adv10b-ground"
if (Test-Path $scratch) { Remove-Item -Recurse -Force $scratch }
Copy-Item -Recurse (Join-Path $repo "ref_ground") $scratch
Copy-Item (Join-Path $repo "playground\adv10b\evil.tsv") (Join-Path $scratch "packs\evil.tsv")
Copy-Item (Join-Path $repo "playground\adv10b\registry-evil.json") (Join-Path $scratch "packs\registry.json") -Force
$arts = Join-Path $env:TEMP "adv10b-artifacts"
if (Test-Path $arts) { Remove-Item -Recurse -Force $arts }
New-Item -ItemType Directory $arts | Out-Null
$env:BT_CONFIG = Join-Path $repo "config\config.json"
$env:BT_REF_GROUND = $scratch
$env:BT_ARTIFACTS = $arts
Write-Output "== probe B+C: evil registry (nspec overflow + tag_of garbage err)"
& (Join-Path $repo "build\bin\verify.exe") 2>&1 | ForEach-Object { $_.ToString() }
Write-Output ("exit: " + $LASTEXITCODE)
Write-Output ("packs.md exists: " + (Test-Path (Join-Path $arts "verify\packs.md")))
Write-Output "== probe A: broken registry (load failure path)"
Set-Content (Join-Path $scratch "packs\registry.json") -Value "not json" -NoNewline
Remove-Item -Recurse -Force (Join-Path $arts "verify") -ErrorAction SilentlyContinue
& (Join-Path $repo "build\bin\verify.exe") 2>&1 | ForEach-Object { $_.ToString() }
Write-Output ("exit: " + $LASTEXITCODE)
Write-Output ("packs.md exists: " + (Test-Path (Join-Path $arts "verify\packs.md")))
