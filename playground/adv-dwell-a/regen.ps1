$repo = 'C:/Users/lavantien/dev/github/blind-trainer'
Set-Location $repo
$env:BT_CONFIG = Join-Path $repo 'config/config.json'
$env:BT_REF_GROUND = 'ref_ground'
$env:BT_ARTIFACTS = 'artifacts'
"BT_CONFIG=$env:BT_CONFIG"
& (Join-Path $repo 'build/bin/memofreq.exe') 2>&1 | ForEach-Object { "mf: $_" }
"memofreq exit $LASTEXITCODE"
$h = (Get-FileHash 'data/ga/freq.json' -Algorithm SHA256).Hash.ToLower()
"freq.json $h"
$golden = (Get-FileHash 'ref_ground/ga/freq.json' -Algorithm SHA256).Hash.ToLower()
"golden freq $golden"
& (Join-Path $repo 'build/bin/ga-run.exe') 2>&1 | ForEach-Object { "gr: $_" }
"ga-run exit $LASTEXITCODE"
foreach ($f in 'data/ga/syllables.json', 'data/ga/report.json', 'artifacts/ga/report.md') {
    $h = (Get-FileHash $f -Algorithm SHA256).Hash.ToLower()
    "$f $h"
}
$a = Get-FileHash 'data/ga/report.json' -Algorithm SHA256
$b = Get-FileHash 'ref_ground/ga/report.json' -Algorithm SHA256
"report_vs_golden_same $($a.Hash -eq $b.Hash)"
