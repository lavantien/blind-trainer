$here = $PSScriptRoot
$repo = Split-Path -Parent (Split-Path -Parent $here)
$log = Join-Path $here 'behavior.log'
$lines = @()
function Snap($label) {
    $rows = @()
    foreach ($rel in @('artifacts/verify/report.md','artifacts/verify/report.json','artifacts/verify/packs.md','data/ga/freq.json','data/ga/syllables.json','data/ga/report.json')) {
        $p = Join-Path $repo $rel
        if (Test-Path $p) {
            $h = (Get-FileHash $p -Algorithm SHA256).Hash.Substring(0,16)
            $rows += $rel + ' ' + (Get-Item $p).Length + ' ' + $h
        } else {
            $rows += $rel + ' MISSING'
        }
    }
    $script:lines += ('SNAP_' + $label)
    $script:lines += $rows
}
$env:BT_REF_GROUND = 'ref_ground'
$env:BT_CONFIG = (Join-Path $repo 'config/config.json')
$env:BT_ARTIFACTS = 'artifacts'
Snap 'BEFORE'
Push-Location $repo
$lines += 'RUN verify.exe'
$v = & (Join-Path $repo 'build/bin/verify.exe') 2>&1
$lines += $v
$lines += ('verify_exit=' + $LASTEXITCODE)
$lines += 'RUN memofreq.exe'
$m = & (Join-Path $repo 'build/bin/memofreq.exe') 2>&1
$lines += ($m | Select-Object -Last 4)
$lines += ('memofreq_exit=' + $LASTEXITCODE)
$lines += 'RUN ga-run.exe'
$g = & (Join-Path $repo 'build/bin/ga-run.exe') 2>&1
$lines += ($g | Select-Object -Last 4)
$lines += ('ga_exit=' + $LASTEXITCODE)
Pop-Location
Snap 'AFTER'
$lines | Set-Content -Path $log
Get-Content $log
