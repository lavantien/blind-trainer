param(
    [long]$MinFreeCommitKb = 9437184
)

$ErrorActionPreference = 'Stop'

$AttemptMb = 6144
$SamplePeriodMs = 150
$SampleMaxIterations = 400
$CeilingBytes = 4294967296

$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$wrap = Join-Path $root 'target/release/bt-jobwrap.exe'
$report = Join-Path $root 'artifacts/jobwrap-ceiling-measure.txt'
$samplesPath = Join-Path $root 'artifacts/progress/r1-runaway-samples.csv'
$runLog = Join-Path $root 'artifacts/progress/r1-runaway-run.log'
$stopFile = Join-Path $root 'artifacts/progress/r1-runaway-stop'

function Log($message) {
    Add-Content -Path $report -Value "$message"
    Write-Output $message
}

function Snapshot-Kb {
    $os = Get-CimInstance Win32_OperatingSystem
    [pscustomobject]@{ FreePhysKb = $os.FreePhysicalMemory; FreeCommitKb = $os.FreeVirtualMemory }
}

try {
    Log "ceiling measure start $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    Log "payload attempt ${AttemptMb} MiB, ceiling $CeilingBytes bytes, sample period ${SamplePeriodMs} ms"
    if (-not (Test-Path $wrap)) {
        Log "fail: wrapper missing at $wrap"
        exit 5
    }
    Remove-Item -Path $samplesPath, $stopFile -ErrorAction SilentlyContinue
    $pre = Snapshot-Kb
    Log "pre free phys kb $($pre.FreePhysKb), free commit kb $($pre.FreeCommitKb)"
    if ($pre.FreeCommitKb -lt $MinFreeCommitKb) {
        Log "abort: free commit kb $($pre.FreeCommitKb) below gate $MinFreeCommitKb, refusing to stress the box"
        exit 4
    }
    $sampler = Start-Job -ScriptBlock {
        param($Path, $PeriodMs, $MaxIterations, $StopFile)
        for ($i = 0; $i -lt $MaxIterations; $i++) {
            $stamp = (Get-Date).ToString('o')
            foreach ($proc in Get-Process -Name bt-jobwrap -ErrorAction SilentlyContinue) {
                Add-Content -Path $Path -Value "$stamp,$($proc.Id),$($proc.PrivateMemorySize64)"
            }
            if (Test-Path $StopFile) { break }
            Start-Sleep -Milliseconds $PeriodMs
        }
    } -ArgumentList $samplesPath, $SamplePeriodMs, $SampleMaxIterations, $stopFile
    Log "sampler job $($sampler.Id) armed at ${SamplePeriodMs} ms"
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    & $wrap --anchor runaway -- $wrap --selftest-alloc-mb $AttemptMb *> $runLog
    $payloadExit = $LASTEXITCODE
    $clock.Stop()
    New-Item -Path $stopFile -ItemType File -Force | Out-Null
    Wait-Job -Job $sampler -Timeout 30 | Out-Null
    Stop-Job -Job $sampler -ErrorAction SilentlyContinue
    $null = Receive-Job -Job $sampler
    Remove-Job -Job $sampler -Force
    $post = Snapshot-Kb
    $peak = 0
    $count = 0
    if (Test-Path $samplesPath) {
        $rows = Import-Csv -Path $samplesPath -Header stamp, pid, private_bytes
        $count = @($rows).Count
        foreach ($row in $rows) {
            $value = [int64]$row.private_bytes
            if ($value -gt $peak) { $peak = $value }
        }
    }
    $wall = [math]::Round($clock.Elapsed.TotalSeconds, 3)
    $peakMiB = [math]::Round($peak / 1MB, 1)
    $ceilingMiB = [math]::Round($CeilingBytes / 1MB, 1)
    Log "wall seconds to death: $wall"
    Log "wrapper forward exit code: $payloadExit (0x$('{0:X8}' -f $payloadExit))"
    Log "peak private commit observed: $peak bytes ($peakMiB MiB) over $count samples"
    Log "ceiling under test: $ceilingMiB MiB"
    Log "post free phys kb $($post.FreePhysKb), free commit kb $($post.FreeCommitKb)"
    Log "machine health: free commit recovered delta kb $($post.FreeCommitKb - $pre.FreeCommitKb)"
    Log "run log at $runLog"
    if ($payloadExit -eq 0) {
        Log "verdict: FAIL, payload survived the attempt past the ceiling"
        exit 6
    }
    if ($peak -gt ($CeilingBytes + 512MB)) {
        Log "verdict: FAIL, peak exceeded ceiling plus slack"
        exit 7
    }
    if ($post.FreeCommitKb -lt ($pre.FreeCommitKb - 524288)) {
        Log "verdict: FAIL, commit charge did not return after the kill"
        exit 8
    }
    Log "verdict: PASS, payload died at the ceiling with exit $payloadExit in $wall s, peak $peakMiB MiB, box stayed healthy"
    Log "ceiling measure end $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    exit 0
}
catch {
    Log "fail: $($_.Exception.Message)"
    Log "ceiling measure aborted $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    exit 9
}
