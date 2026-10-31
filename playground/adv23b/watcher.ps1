param(
    [string]$WatchLog,
    [string]$StopFile,
    [int]$DeadlineSec = 1800
)
$deadline = (Get-Date).AddSeconds($DeadlineSec)
$samples = 0
$names = @{}
while (-not (Test-Path $StopFile) -and (Get-Date) -lt $deadline) {
    $procs = Get-Process -Name nvcc*,cicc*,ptxas*,cudafe*,clang* -ErrorAction SilentlyContinue
    foreach ($p in $procs) {
        $names[$p.ProcessName] = ([int]$names[$p.ProcessName]) + 1
    }
    if ($procs) {
        Add-Content $WatchLog ("{0} {1}" -f (Get-Date -Format 'HH:mm:ss.fff'), (($procs | ForEach-Object ProcessName | Sort-Object) -join ','))
    }
    $samples++
    Start-Sleep -Milliseconds 120
}
"done samples=$samples saw=$($names.Keys -join ',')" | Add-Content $WatchLog
