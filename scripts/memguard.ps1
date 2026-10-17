param(
    [long]$CapBytes = 0,
    [switch]$Kill,
    [switch]$SelfTest,
    [string]$SleeperPath = ""
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoNorm = $repo.Replace('\', '/').ToLowerInvariant().TrimEnd('/') + '/'

function Write-Fail([string]$message) {
    [Console]::Error.WriteLine("memguard: $message")
}

function Format-MegaBytes([double]$value) {
    '{0:F1}' -f ($value / 1MB)
}

function Resolve-ExePath([string]$path) {
    if ($null -eq $path -or $path.Length -eq 0) { return $null }
    if (-not $path.Contains('~')) { return $path }
    try { return (Get-Item -LiteralPath $path -ErrorAction Stop).FullName } catch { return $path }
}

function Get-RepoProcess {
    $rows = @()
    foreach ($proc in Get-CimInstance Win32_Process) {
        if ($proc.ProcessId -eq $PID) { continue }
        $exe = Resolve-ExePath $proc.ExecutablePath
        if ($null -eq $exe) { continue }
        $norm = $exe.Replace('\', '/').ToLowerInvariant()
        if (-not $norm.StartsWith($repoNorm)) { continue }
        $live = Get-Process -Id $proc.ProcessId -ErrorAction SilentlyContinue
        if ($null -eq $live) { continue }
        $rows += [pscustomobject]@{ Id = $live.Id; Name = $live.ProcessName; WS = [long]$live.WorkingSet64 }
    }
    return $rows
}

function Wait-ProcessGone([int]$id, [double]$seconds) {
    $deadline = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $deadline) {
        if ($null -eq (Get-Process -Id $id -ErrorAction SilentlyContinue)) { return $true }
        Start-Sleep -Milliseconds 200
    }
    return $null -eq (Get-Process -Id $id -ErrorAction SilentlyContinue)
}

function Get-MemguardConfig {
    $envConfig = $env:BT_CONFIG
    $configPath = if ($null -ne $envConfig -and $envConfig -ne '') {
        if ([System.IO.Path]::IsPathRooted($envConfig)) { $envConfig } else { Join-Path $repo $envConfig }
    } else {
        Join-Path $repo 'config' 'config.json'
    }
    $script:MemguardConfigPath = $configPath
    if (-not (Test-Path -LiteralPath $configPath)) {
        Write-Fail "config not found: $configPath, set -CapBytes or add memguard.cap_bytes"
        exit 2
    }
    try {
        return Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json -AsHashtable
    } catch {
        Write-Fail "config unreadable: $configPath : $_"
        exit 2
    }
}

function Get-MemguardValue([string]$key) {
    $config = Get-MemguardConfig
    if (-not $config.ContainsKey('memguard') -or $null -eq $config['memguard'][$key]) {
        Write-Fail "config section missing: memguard.$key in $script:MemguardConfigPath"
        exit 2
    }
    return $config['memguard'][$key]
}

function Resolve-CapBytes([long]$override) {
    if ($override -gt 0) { return $override }
    if ($override -lt 0) {
        Write-Fail "-CapBytes must be a positive byte count"
        exit 2
    }
    $cap = 0L
    if (-not [long]::TryParse([string](Get-MemguardValue 'cap_bytes'), [ref]$cap) -or $cap -le 0) {
        Write-Fail "config value invalid: memguard.cap_bytes in $script:MemguardConfigPath"
        exit 2
    }
    return $cap
}

function Resolve-PidDir {
    $pidDir = [string](Get-MemguardValue 'pid_dir')
    if ($pidDir.Trim().Length -eq 0) {
        Write-Fail "config value invalid: memguard.pid_dir in $script:MemguardConfigPath"
        exit 2
    }
    if ([System.IO.Path]::IsPathRooted($pidDir)) { return $pidDir }
    return (Join-Path $repo $pidDir)
}

function Get-PidFileRoots([string]$pidDir) {
    $roots = @()
    if (-not (Test-Path -LiteralPath $pidDir)) { return $roots }
    foreach ($file in @(Get-ChildItem -LiteralPath $pidDir -Filter '*.pid' -File -ErrorAction SilentlyContinue)) {
        try { $raw = Get-Content -LiteralPath $file.FullName -Raw } catch { continue }
        if ($null -eq $raw) { continue }
        $value = 0L
        if (-not [long]::TryParse($raw.Trim(), [ref]$value) -or $value -le 0 -or $value -gt [int]::MaxValue) { continue }
        if ($value -eq $PID) { continue }
        if ($null -eq (Get-Process -Id $value -ErrorAction SilentlyContinue)) { continue }
        $roots += [int]$value
    }
    return $roots
}

function Get-ProcessChildIndex {
    $index = @{}
    foreach ($proc in Get-CimInstance Win32_Process) {
        $parent = [int]$proc.ParentProcessId
        if (-not $index.ContainsKey($parent)) { $index[$parent] = [System.Collections.Generic.List[int]]::new() }
        $index[$parent].Add([int]$proc.ProcessId)
    }
    return $index
}

function Get-TreeProcessIds([int[]]$roots, $index) {
    $seen = @{}
    $queue = [System.Collections.Generic.Queue[int]]::new()
    foreach ($root in $roots) {
        if (-not $seen.ContainsKey($root)) { $seen[$root] = $true; $queue.Enqueue($root) }
    }
    while ($queue.Count -gt 0) {
        $current = $queue.Dequeue()
        if (-not $index.ContainsKey($current)) { continue }
        foreach ($child in $index[$current]) {
            if (-not $seen.ContainsKey($child)) { $seen[$child] = $true; $queue.Enqueue($child) }
        }
    }
    return $seen.Keys
}

function Get-PidTreeProcess([string]$pidDir) {
    $rows = @()
    $roots = @(Get-PidFileRoots $pidDir)
    if ($roots.Count -eq 0) { return $rows }
    $index = Get-ProcessChildIndex
    foreach ($id in @(Get-TreeProcessIds $roots $index)) {
        if ($id -eq $PID) { continue }
        $live = Get-Process -Id $id -ErrorAction SilentlyContinue
        if ($null -eq $live) { continue }
        $rows += [pscustomobject]@{ Id = $live.Id; Name = $live.ProcessName; WS = [long]$live.WorkingSet64 }
    }
    return $rows
}

function Get-TrackedProcess([string]$pidDir) {
    $rows = @(@(Get-RepoProcess) + @(Get-PidTreeProcess $pidDir))
    return @($rows | Sort-Object Id -Unique)
}

function Invoke-Monitor([object[]]$rows, [long]$cap, [bool]$kill) {
    $total = [long](($rows | Measure-Object -Property WS -Sum).Sum)
    if ($total -le $cap) {
        Write-Host ("memguard: {0} procs {1} MB / {2} MB cap ok" -f $rows.Count, (Format-MegaBytes $total), (Format-MegaBytes $cap))
        return 0
    }
    $largest = $rows | Sort-Object WS -Descending | Select-Object -First 1
    $breach = "{0} procs {1} MB / {2} MB cap breach, largest pid {3} {4} {5} MB" -f $rows.Count, (Format-MegaBytes $total), (Format-MegaBytes $cap), $largest.Id, $largest.Name, (Format-MegaBytes $largest.WS)
    if (-not $kill) {
        Write-Fail $breach
        return 3
    }
    Write-Host "memguard: $breach"
    taskkill /PID $largest.Id /T /F | Out-Null
    if (Wait-ProcessGone $largest.Id 5) {
        Write-Host "memguard: killed pid $($largest.Id) $($largest.Name), verified gone"
        return 3
    }
    Write-Fail "kill failed for pid $($largest.Id) $($largest.Name), still alive"
    return 5
}

function Invoke-MemguardSelfTest {
    if ($SleeperPath -eq '' -or -not (Test-Path -LiteralPath $SleeperPath)) {
        Write-Fail "selftest sleeper binary not found: $SleeperPath"
        return 4
    }
    $sleeperFull = (Resolve-Path -LiteralPath $SleeperPath).Path
    if (-not $sleeperFull.StartsWith($repo, [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Fail "selftest sleeper binary is outside the repo: $sleeperFull"
        return 4
    }
    $tempDir = Join-Path $repo ('.memguard-selftest-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $tempDir | Out-Null
    $sleeper = $null
    $decoy = $null
    $treeSleeper = $null
    $treeRoot = $null
    $treeChildId = 0
    $outsideDir = ''
    $code = 5
    try {
        $pwshExe = (Get-Command pwsh).Source
        $copyExe = Join-Path $tempDir 'bt-sleeper.exe'
        Copy-Item -LiteralPath $sleeperFull -Destination $copyExe
        $launchExe = $copyExe
        try {
            $short = (New-Object -ComObject Scripting.FileSystemObject).GetFile($copyExe).ShortPath
            if ($null -ne $short -and $short -ne '') { $launchExe = $short }
        } catch { }
        $sleeper = Start-Process -FilePath $launchExe -ArgumentList '120' -PassThru -WindowStyle Hidden
        $decoyCommand = "Write-Output '$repo'; Start-Sleep 90"
        $decoy = Start-Process -FilePath $pwshExe -ArgumentList @('-NoProfile', '-Command', $decoyCommand) -PassThru -WindowStyle Hidden
        $cmdLine = $null
        for ($i = 0; $i -lt 20; $i++) {
            $cmdLine = (Get-CimInstance Win32_Process -Filter "ProcessId = $($decoy.Id)").CommandLine
            if ($null -ne $cmdLine -and $cmdLine.Contains($repo)) { break }
            Start-Sleep -Milliseconds 250
        }
        if ($null -eq $cmdLine -or -not $cmdLine.Contains($repo)) {
            Write-Fail "selftest decoy pid $($decoy.Id) command line does not carry the repo path"
            return 5
        }
        Write-Host "memguard selftest ok: decoy pid $($decoy.Id) command line carries the repo path, exe is $pwshExe"
        $sleeperSeen = $false
        for ($i = 0; $i -lt 20; $i++) {
            $rows = @(Get-RepoProcess)
            if ($null -ne ($rows | Where-Object { $_.Id -eq $sleeper.Id })) { $sleeperSeen = $true; break }
            Start-Sleep -Milliseconds 250
        }
        if (-not $sleeperSeen) {
            Write-Fail "selftest sleeper pid $($sleeper.Id) not matched by exe path, launched as $launchExe"
            return 5
        }
        Write-Host "memguard selftest ok: sleeper pid $($sleeper.Id) matched via exe path, launched as $launchExe"
        $rows = @(Get-RepoProcess)
        $decoyCounted = $null -ne ($rows | Where-Object { $_.Id -eq $decoy.Id })
        if ($decoyCounted) {
            Write-Fail "selftest decoy pid $($decoy.Id) matched, command line matching leaked into the matcher"
            return 5
        }
        Write-Host "memguard selftest ok: decoy pid $($decoy.Id) not matched"
        $pidDir = Join-Path $tempDir 'pids'
        New-Item -ItemType Directory -Path $pidDir | Out-Null
        $resolvedPidDir = Resolve-PidDir
        if ([string]::IsNullOrWhiteSpace($resolvedPidDir)) {
            Write-Fail "selftest resolved an empty memguard.pid_dir"
            return 5
        }
        Write-Host "memguard selftest ok: memguard.pid_dir resolved from config, $resolvedPidDir"
        $outsideDir = Join-Path ([System.IO.Path]::GetTempPath()) ('bt-memguard-selftest-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $outsideDir | Out-Null
        $outsideExe = Join-Path $outsideDir 'bt-sleeper.exe'
        Copy-Item -LiteralPath $sleeperFull -Destination $outsideExe
        $treeSleeper = Start-Process -FilePath $outsideExe -ArgumentList '120' -PassThru -WindowStyle Hidden
        $deadProc = Start-Process -FilePath $pwshExe -ArgumentList @('-NoProfile', '-Command', 'exit 0') -PassThru -WindowStyle Hidden
        $treeRoot = Start-Process -FilePath $pwshExe -ArgumentList @('-NoProfile', '-Command', "& '$outsideExe' 120") -PassThru -WindowStyle Hidden
        if (-not (Wait-ProcessGone $deadProc.Id 10)) {
            Write-Fail "selftest stale pid $($deadProc.Id) still alive"
            return 5
        }
        Set-Content -LiteralPath (Join-Path $pidDir 'selftest-stale.pid') -Value $deadProc.Id
        Set-Content -LiteralPath (Join-Path $pidDir 'selftest-sleeper.pid') -Value $treeSleeper.Id
        $treeChildSeen = $false
        for ($i = 0; $i -lt 40; $i++) {
            $child = Get-CimInstance Win32_Process -Filter "ParentProcessId = $($treeRoot.Id)" | Where-Object { $_.Name -like 'bt-sleeper*' } | Select-Object -First 1
            if ($null -ne $child) { $treeChildSeen = $true; $treeChildId = [int]$child.ProcessId; break }
            Start-Sleep -Milliseconds 250
        }
        if (-not $treeChildSeen) {
            Write-Fail "selftest tree root pid $($treeRoot.Id) never spawned the child sleeper"
            return 5
        }
        Set-Content -LiteralPath (Join-Path $pidDir 'selftest-tree.pid') -Value $treeRoot.Id
        Start-Sleep -Milliseconds 500
        $rows = @(Get-TrackedProcess $pidDir)
        foreach ($expected in @($treeSleeper.Id, $treeRoot.Id, $treeChildId)) {
            if ($null -eq ($rows | Where-Object { $_.Id -eq $expected })) {
                Write-Fail "selftest pid $expected not matched via pidfile tree, exe outside repo at $outsideExe"
                return 5
            }
        }
        Write-Host "memguard selftest ok: pidfile tree matched pids $($treeSleeper.Id), $($treeRoot.Id), $($treeChildId)"
        if ($null -ne ($rows | Where-Object { $_.Id -eq $deadProc.Id })) {
            Write-Fail "selftest stale pid $($deadProc.Id) matched from a dead pidfile"
            return 5
        }
        Write-Host "memguard selftest ok: stale pidfile with dead pid $($deadProc.Id) ignored"
        $decoyCounted = $null -ne ($rows | Where-Object { $_.Id -eq $decoy.Id })
        if ($decoyCounted) {
            Write-Fail "selftest decoy pid $($decoy.Id) matched, command line matching leaked into the pidtree matcher"
            return 5
        }
        Write-Host "memguard selftest ok: decoy pid $($decoy.Id) not matched by pidtree"
        $live = Get-Process -Id $treeSleeper.Id -ErrorAction SilentlyContinue
        if ($null -eq $live) {
            Write-Fail "selftest pidfile sleeper pid $($treeSleeper.Id) died before the kill fixture"
            return 5
        }
        $fixtureRows = @([pscustomobject]@{ Id = $live.Id; Name = $live.ProcessName; WS = [long]$live.WorkingSet64 })
        $noKill = Invoke-Monitor $fixtureRows 1 $false
        if ($noKill -ne 3) {
            Write-Fail "selftest breach without -Kill returned $noKill"
            return 5
        }
        if ($null -eq (Get-Process -Id $treeSleeper.Id -ErrorAction SilentlyContinue)) {
            Write-Fail "selftest breach without -Kill touched pid $($treeSleeper.Id)"
            return 5
        }
        Write-Host "memguard selftest ok: breach without -Kill left pid $($treeSleeper.Id) alive"
        $live = Get-Process -Id $treeSleeper.Id -ErrorAction SilentlyContinue
        $fixtureRows = @([pscustomobject]@{ Id = $live.Id; Name = $live.ProcessName; WS = [long]$live.WorkingSet64 })
        $withKill = Invoke-Monitor $fixtureRows 1 $true
        if ($withKill -ne 3 -or -not (Wait-ProcessGone $treeSleeper.Id 5)) {
            Write-Fail "selftest -Kill returned $withKill and left pid $($treeSleeper.Id) unverified"
            return 5
        }
        Write-Host "memguard selftest ok: -Kill killed pidfile pid $($treeSleeper.Id), verified gone"
        $code = 0
    } finally {
        $sleeperGone = $true
        $decoyGone = $true
        $treeGone = $true
        if ($null -ne $sleeper) {
            Stop-Process -Id $sleeper.Id -Force -ErrorAction SilentlyContinue
            $sleeperGone = Wait-ProcessGone $sleeper.Id 5
            if ($sleeperGone) { Write-Host "memguard selftest ok: sleeper pid $($sleeper.Id) killed and gone" }
        }
        if ($null -ne $decoy) {
            Stop-Process -Id $decoy.Id -Force -ErrorAction SilentlyContinue
            $decoyGone = Wait-ProcessGone $decoy.Id 5
            if ($decoyGone) { Write-Host "memguard selftest ok: decoy pid $($decoy.Id) killed and gone" }
        }
        if ($null -ne $treeRoot) {
            taskkill /PID $treeRoot.Id /T /F 2>$null | Out-Null
            $treeGone = (Wait-ProcessGone $treeRoot.Id 5)
            if ($treeChildId -gt 0) { $treeGone = $treeGone -and (Wait-ProcessGone $treeChildId 5) }
            if ($treeGone) { Write-Host "memguard selftest ok: tree root pid $($treeRoot.Id) and child killed and gone" }
        }
        if ($null -ne $treeSleeper) {
            Stop-Process -Id $treeSleeper.Id -Force -ErrorAction SilentlyContinue
            $treeGone = $treeGone -and (Wait-ProcessGone $treeSleeper.Id 5)
        }
        if ($outsideDir -ne '' -and (Test-Path -LiteralPath $outsideDir)) {
            Remove-Item -LiteralPath $outsideDir -Recurse -Force -ErrorAction SilentlyContinue
        }
        $outsideGone = ($outsideDir -eq '') -or (-not (Test-Path -LiteralPath $outsideDir))
        $tempGone = -not (Test-Path -LiteralPath $tempDir)
        if (-not $tempGone) { Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue }
        $tempGone = -not (Test-Path -LiteralPath $tempDir)
        if (-not $sleeperGone -or -not $decoyGone -or -not $treeGone -or -not $outsideGone -or -not $tempGone) {
            Write-Fail "selftest cleanup failed"
            $code = 5
        }
    }
    return $code
}

if ($SelfTest) {
    exit (Invoke-MemguardSelfTest)
}

$cap = Resolve-CapBytes $CapBytes
$rows = @(Get-TrackedProcess (Resolve-PidDir))
exit (Invoke-Monitor $rows $cap $Kill.IsPresent)
