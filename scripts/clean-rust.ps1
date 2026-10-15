param(
    [int]$StaleHours = -1,
    [switch]$Apply,
    [switch]$Deep,
    [switch]$OrphanScratch,
    [switch]$SelfTest,
    [string]$WorkDir = "",
    [string]$RepoRoot = "",
    [string]$TempDir = "",
    [string]$CoverageTarget = "",
    [string]$MutationTarget = ""
)

$ErrorActionPreference = 'Stop'

function Write-Fail([string]$message) {
    [Console]::Error.WriteLine("clean-rust: $message")
}

function Format-GigaBytes([double]$value) {
    '{0:F2}' -f ($value / 1GB)
}

function Resolve-RepoRoot([string]$override) {
    if ($override -ne '') { return (Resolve-Path -LiteralPath $override).Path }
    return (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
}

function Resolve-ConfigPath([string]$repo) {
    $envConfig = $env:BT_CONFIG
    if ($null -ne $envConfig -and $envConfig -ne '') {
        if ([System.IO.Path]::IsPathRooted($envConfig)) { return $envConfig }
        return (Join-Path $repo $envConfig)
    }
    return (Join-Path $repo 'config' 'config.json')
}

function Resolve-DiskSettings([int]$override, [string]$repo) {
    $path = Resolve-ConfigPath $repo
    if (-not (Test-Path -LiteralPath $path)) {
        Write-Fail "config not found: $path"
        exit 2
    }
    try {
        $doc = Get-Content -Raw -LiteralPath $path | ConvertFrom-Json
    } catch {
        Write-Fail "config parse failed: $($_.Exception.Message)"
        exit 2
    }
    if ($null -eq $doc.disk -or $null -eq $doc.disk.scratch_stale_hours -or $null -eq $doc.disk.free_floor_gb) {
        Write-Fail "config disk.scratch_stale_hours or disk.free_floor_gb missing"
        exit 2
    }
    $stale = [int]$doc.disk.scratch_stale_hours
    if ($override -ge 0) { $stale = $override }
    return @{ StaleHours = $stale; FloorGb = [double]$doc.disk.free_floor_gb }
}

function Get-DirBytes([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return [long]0 }
    $sum = [long]0
    Get-ChildItem -LiteralPath $path -Recurse -File -Force -ErrorAction SilentlyContinue | ForEach-Object { $sum += $_.Length }
    return $sum
}

function Get-StaleScratches([string]$temp, [int]$staleHours) {
    $cutoff = (Get-Date).AddHours(-$staleHours)
    @(Get-ChildItem -LiteralPath $temp -Directory -Filter 'cargo-mutants*' -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -lt $cutoff } |
        ForEach-Object { $_.FullName })
}

function Get-OrphanScratches([string]$temp) {
    $live = @(Get-Process -Name 'cargo-mutants' -ErrorAction SilentlyContinue)
    $all = @(Get-ChildItem -LiteralPath $temp -Directory -Filter 'cargo-mutants*' -ErrorAction SilentlyContinue)
    if ($live.Count -eq 0) { return @($all | ForEach-Object { $_.FullName }) }
    $oldestStart = ($live | Measure-Object StartTime -Minimum).Minimum
    @($all |
        Where-Object { $_.CreationTime -lt $oldestStart.AddSeconds(-5) } |
        ForEach-Object { $_.FullName })
}

function Remove-Incremental([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return }
    Get-ChildItem -LiteralPath $path -Recurse -File -Force -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
    Get-ChildItem -LiteralPath $path -Force -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $path -Force -Recurse -ErrorAction SilentlyContinue
}

function Remove-Tree([string]$path) {
    try {
        Remove-Item -LiteralPath $path -Recurse -Force -ErrorAction Stop
    } catch {
        Write-Fail "delete failed: ${path}: $($_.Exception.Message)"
        return $false
    }
    return -not (Test-Path -LiteralPath $path)
}

function Invoke-SelfTest {
    if ($WorkDir -eq '') {
        Write-Fail 'selftest needs -WorkDir'
        exit 2
    }
    if (Test-Path -LiteralPath $WorkDir) { Remove-Item -LiteralPath $WorkDir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
    $work = (Resolve-Path -LiteralPath $WorkDir).Path
    $repoFake = Join-Path $work 'repo'
    $inc = Join-Path $repoFake 'target/debug/incremental'
    New-Item -ItemType Directory -Force -Path $inc | Out-Null
    Set-Content -Path (Join-Path $inc 'cache.bin') -Value 'x'
    $tempFake = Join-Path $work 'temp'
    New-Item -ItemType Directory -Force -Path $tempFake | Out-Null
    $staleDir = Join-Path $tempFake 'cargo-mutants-stale'
    $freshDir = Join-Path $tempFake 'cargo-mutants-fresh'
    New-Item -ItemType Directory -Force -Path $staleDir, $freshDir | Out-Null
    Set-Content -Path (Join-Path $staleDir 'a') -Value 'x'
    Set-Content -Path (Join-Path $freshDir 'a') -Value 'x'
    (Get-Item -LiteralPath $staleDir).LastWriteTime = (Get-Date).AddHours(-8)
    (Get-Item -LiteralPath $freshDir).LastWriteTime = (Get-Date).AddHours(-1)
    $ok = $true
    $scratches = @(Get-StaleScratches $tempFake 6)
    if ($scratches.Count -ne 1 -or $scratches[0] -ne $staleDir) {
        Write-Fail "stale selection wrong: $($scratches.Count) picked, expected exactly cargo-mutants-stale"
        $ok = $false
    }
    Remove-Incremental $inc
    if (Test-Path -LiteralPath (Join-Path $inc 'cache.bin')) {
        Write-Fail 'incremental file survived apply'
        $ok = $false
    }
    if (-not (Remove-Tree $staleDir)) {
        Write-Fail 'stale scratch delete failed'
        $ok = $false
    }
    if (-not (Test-Path -LiteralPath $freshDir)) {
        Write-Fail 'fresh scratch was deleted, active run would be destroyed'
        $ok = $false
    }
    if ($ok) { Write-Output 'clean-rust: selftest ok' }
    if (-not $ok) { exit 1 }
    exit 0
}

if ($SelfTest) { Invoke-SelfTest }

if ($StaleHours -lt -1) {
    Write-Fail '-StaleHours must be 0 or greater'
    exit 2
}

$repo = Resolve-RepoRoot $RepoRoot
$settings = Resolve-DiskSettings $StaleHours $repo
$temp = if ($TempDir -ne '') { $TempDir } else { $env:TEMP }
$cov = if ($CoverageTarget -ne '') { $CoverageTarget } else { Join-Path (Split-Path $repo) 'bt-coverage-target' }
$mut = if ($MutationTarget -ne '') { $MutationTarget } else { Join-Path (Split-Path $repo) 'bt-mutation-target' }

$drive = Get-PSDrive -Name $repo.Substring(0, 1)
$freeBefore = [double]$drive.Free

$incrementals = @((Join-Path $repo 'target/debug/incremental'), (Join-Path $repo 'target/release/incremental')) |
    Where-Object { Test-Path -LiteralPath $_ }
$scratches = @(Get-StaleScratches $temp $settings.StaleHours)
if ($OrphanScratch) {
    $scratches = @($scratches + @(Get-OrphanScratches $temp) | Select-Object -Unique)
}
$externals = @($cov, $mut) | Where-Object { Test-Path -LiteralPath $_ }
$deepDirs = @((Join-Path $repo 'tests/e2e/target'), (Join-Path $repo 'fuzz/target')) |
    Where-Object { Test-Path -LiteralPath $_ }

$incBytes = ($incrementals | ForEach-Object { Get-DirBytes $_ } | Measure-Object -Sum).Sum
$scratchBytes = ($scratches | ForEach-Object { Get-DirBytes $_ } | Measure-Object -Sum).Sum
$extBytes = ($externals | ForEach-Object { Get-DirBytes $_ } | Measure-Object -Sum).Sum
$deepBytes = ($deepDirs | ForEach-Object { Get-DirBytes $_ } | Measure-Object -Sum).Sum

Write-Output ("clean-rust: free {0} GB floor {1} GB" -f (Format-GigaBytes $freeBefore), $settings.FloorGb)
Write-Output ("incremental: {0} dirs {1} GB" -f $incrementals.Count, (Format-GigaBytes $incBytes))
Write-Output ("scratch: {0} dirs {1} GB (cargo-mutants under TEMP older than {2} h{3})" -f $scratches.Count, (Format-GigaBytes $scratchBytes), $settings.StaleHours, $(if ($OrphanScratch) { ", orphans with no live owner included" } else { "" }))
Write-Output ("external: {0} dirs {1} GB (coverage and mutation targets)" -f $externals.Count, (Format-GigaBytes $extBytes))
Write-Output ("deep: {0} dirs {1} GB (e2e and fuzz targets, needs -Deep and no live cargo or rustc)" -f $deepDirs.Count, (Format-GigaBytes $deepBytes))

if ($freeBefore / 1GB -lt $settings.FloorGb) {
    Write-Output ("clean-rust: WARN below {0} GB floor" -f $settings.FloorGb)
}

if (-not $Apply) {
    Write-Output ("reclaimable now {0} GB (run with -Apply)" -f (Format-GigaBytes ($incBytes + $scratchBytes + $extBytes)))
    exit 0
}

if ($Deep) {
    $live = @(Get-Process cargo, rustc -ErrorAction SilentlyContinue)
    if ($live.Count -gt 0) {
        Write-Fail "deep clean refused: $($live.Count) cargo or rustc processes live"
        exit 3
    }
}

foreach ($inc in $incrementals) { Remove-Incremental $inc }
foreach ($scratch in $scratches) {
    if (-not (Remove-Tree $scratch)) { exit 1 }
}
foreach ($ext in $externals) {
    if (-not (Remove-Tree $ext)) { exit 1 }
}
if ($Deep) {
    foreach ($dir in $deepDirs) {
        if (-not (Remove-Tree $dir)) { exit 1 }
    }
}

$freeAfter = [double](Get-PSDrive -Name $repo.Substring(0, 1)).Free
Write-Output ("clean-rust: freed {0} GB free {1} GB" -f (Format-GigaBytes ($freeAfter - $freeBefore)), (Format-GigaBytes $freeAfter))
exit 0
