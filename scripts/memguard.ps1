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

function Resolve-CapBytes([long]$override) {
    if ($override -gt 0) { return $override }
    if ($override -lt 0) {
        Write-Fail "-CapBytes must be a positive byte count"
        exit 2
    }
    $envConfig = $env:BT_CONFIG
    $configPath = if ($null -ne $envConfig -and $envConfig -ne '') {
        if ([System.IO.Path]::IsPathRooted($envConfig)) { $envConfig } else { Join-Path $repo $envConfig }
    } else {
        Join-Path $repo 'config' 'config.json'
    }
    if (-not (Test-Path -LiteralPath $configPath)) {
        Write-Fail "config not found: $configPath, set -CapBytes or add memguard.cap_bytes"
        exit 2
    }
    try {
        $config = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json -AsHashtable
    } catch {
        Write-Fail "config unreadable: $configPath : $_"
        exit 2
    }
    if (-not $config.ContainsKey('memguard') -or $null -eq $config['memguard']['cap_bytes']) {
        Write-Fail "config section missing: memguard.cap_bytes in $configPath"
        exit 2
    }
    $cap = 0L
    if (-not [long]::TryParse([string]$config['memguard']['cap_bytes'], [ref]$cap) -or $cap -le 0) {
        Write-Fail "config value invalid: memguard.cap_bytes in $configPath"
        exit 2
    }
    return $cap
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
    $code = 5
    try {
        $copyExe = Join-Path $tempDir 'bt-sleeper.exe'
        Copy-Item -LiteralPath $sleeperFull -Destination $copyExe
        $launchExe = $copyExe
        try {
            $short = (New-Object -ComObject Scripting.FileSystemObject).GetFile($copyExe).ShortPath
            if ($null -ne $short -and $short -ne '') { $launchExe = $short }
        } catch { }
        $sleeper = Start-Process -FilePath $launchExe -ArgumentList '120' -PassThru -WindowStyle Hidden
        $decoyExe = (Get-Command pwsh).Source
        $decoyCommand = "Write-Output '$repo'; Start-Sleep 90"
        $decoy = Start-Process -FilePath $decoyExe -ArgumentList @('-NoProfile', '-Command', $decoyCommand) -PassThru -WindowStyle Hidden
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
        Write-Host "memguard selftest ok: decoy pid $($decoy.Id) command line carries the repo path, exe is $decoyExe"
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
        $code = 0
    } finally {
        $sleeperGone = $true
        $decoyGone = $true
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
        $tempGone = -not (Test-Path -LiteralPath $tempDir)
        if (-not $tempGone) { Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue }
        $tempGone = -not (Test-Path -LiteralPath $tempDir)
        if (-not $sleeperGone -or -not $decoyGone -or -not $tempGone) {
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
$rows = @(Get-RepoProcess)
$total = [long](($rows | Measure-Object -Property WS -Sum).Sum)

if ($total -le $cap) {
    Write-Host ("memguard: {0} procs {1} MB / {2} MB cap ok" -f $rows.Count, (Format-MegaBytes $total), (Format-MegaBytes $cap))
    exit 0
}

$largest = $rows | Sort-Object WS -Descending | Select-Object -First 1
$breach = "{0} procs {1} MB / {2} MB cap breach, largest pid {3} {4} {5} MB" -f $rows.Count, (Format-MegaBytes $total), (Format-MegaBytes $cap), $largest.Id, $largest.Name, (Format-MegaBytes $largest.WS)

if (-not $Kill) {
    Write-Fail $breach
    exit 3
}

Write-Host "memguard: $breach"
taskkill /PID $largest.Id /T /F
if (Wait-ProcessGone $largest.Id 5) {
    Write-Host "memguard: killed pid $($largest.Id) $($largest.Name), verified gone"
    exit 3
}
Write-Fail "kill failed for pid $($largest.Id) $($largest.Name), still alive"
exit 5
