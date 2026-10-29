$ErrorActionPreference = 'Continue'
$shard = 15
$repo = 'C:\Users\lavantien\dev\github\blind-trainer'
$passLog = Join-Path $env:TEMP ("bt-mut-shard" + $shard + ".log")
$progress = Join-Path $repo 'artifacts\progress\t67.md'
$stateFile = Join-Path $env:TEMP ("bt-mut-shard" + $shard + ".json")
$mutate = Join-Path $repo 'playground\mutcrash\mutate-fixed.exe'
$scratch = Join-Path $env:TEMP ("bt-mutants-shard" + $shard)

function Log($m) { Add-Content -Path $passLog -Value ('{0} {1}' -f (Get-Date -Format s), $m) }

Log ("shard {0} start pid {1} engine {2} scratch {3}" -f $shard, $PID, $mutate, $scratch)
$state = Get-Content $stateFile -Raw | ConvertFrom-Json
$env:BT_CONFIG = $state.env.BT_CONFIG
$env:BT_ARTIFACTS = $state.env.BT_ARTIFACTS
$env:BT_REF_GROUND = $state.env.BT_REF_GROUND
Log ("env pinned, plans={0}" -f $state.plans.Count)

foreach ($p in $state.plans) {
    $suite = $p.suite
    $suiteLog = Join-Path $env:TEMP ("bt-mut-shard" + $shard + "-" + $suite + ".log")
    Log ("suite {0} begin" -f $suite)
    Push-Location $repo
    & $mutate run --scratch $scratch --ledger $p.ledger --exe $suite @($p.sources) *>> $suiteLog
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        Log ("suite {0} attempt 1 exit {1}, backoff 120s then retry" -f $suite, $code)
        Start-Sleep -Seconds 120
        & $mutate run --scratch $scratch --ledger $p.ledger --exe $suite @($p.sources) *>> $suiteLog
        $code = $LASTEXITCODE
        Log ("suite {0} attempt 2 exit {1}" -f $suite, $code)
    }
    if ($code -eq 0) {
        & $mutate report --ledger $p.ledger --out $p.outcomes *>> $suiteLog
        Log ("suite {0} report exit {1}" -f $suite, $LASTEXITCODE)
    }
    Pop-Location
    $led = Join-Path $repo ($p.ledger -replace '/', '\')
    if (Test-Path $led) {
        $rows = @(Get-Content $led | Select-Object -Skip 1)
        $t = @{}
        foreach ($r in $rows) {
            $st = ($r -split "`t")[7]
            if ($st) { $t[$st] = 1 + [int]$t[$st] }
        }
        $line = ('- {0} shard{1} {2}: total={3} killed={4} survived={5} timeout={6} builderr={7} pending={8}' -f (Get-Date -Format s), $shard, $suite, $rows.Count, [int]$t['killed'], [int]$t['survived'], [int]$t['timeout'], [int]$t['builderr'], [int]$t['pending'])
        Add-Content -Path $progress -Value $line
        Log ("suite {0} tally {1}" -f $suite, $line)
    } else {
        Add-Content -Path $progress -Value ('- {0} shard{1} {2}: FAILED exit {3}, no ledger' -f (Get-Date -Format s), $shard, $suite, $code)
    }
}
Log 'shard done'
Add-Content -Path $progress -Value ('- {0} shard{1} host done' -f (Get-Date -Format s), $shard)
