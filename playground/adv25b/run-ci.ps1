param(
    [Parameter(Mandatory = $true)][string]$RepoDir,
    [Parameter(Mandatory = $true)][string]$LogPath
)

$ErrorActionPreference = "Continue"
$env:Path = "C:\Program Files\LLVM\bin;" + $env:Path
$overall = 0

function Step([string]$Name, [scriptblock]$Body, [bool]$Fatal = $false) {
    Write-Output "`n===== STEP $Name =====" | Tee-Object -FilePath $LogPath -Append
    $started = Get-Date
    Push-Location $RepoDir
    try {
        & $Body 2>&1 | Tee-Object -FilePath $LogPath -Append
        $code = $LASTEXITCODE
    } catch {
        Write-Output "EXCEPTION: $_" | Tee-Object -FilePath $LogPath -Append
        $code = 999
    } finally {
        Pop-Location
    }
    $secs = [int]((Get-Date) - $started).TotalSeconds
    Write-Output ("===== STEP $Name exit=$code secs=$secs =====") | Tee-Object -FilePath $LogPath -Append
    if ($code -ne 0 -and $overall -eq 0) { $script:overall = $code }
    if ($code -ne 0 -and $Fatal) {
        Write-Output "FATAL step failed, stopping" | Tee-Object -FilePath $LogPath -Append
        exit $code
    }
}

Set-Location $RepoDir
"adv25b fresh-clone ci simulation" | Tee-Object -FilePath $LogPath
"repo: $RepoDir" | Tee-Object -FilePath $LogPath -Append
"head: $(git rev-parse HEAD)" | Tee-Object -FilePath $LogPath -Append
"llvm: $((clang --version | Select-Object -First 1))" | Tee-Object -FilePath $LogPath -Append
"python: $(python --version 2>&1)" | Tee-Object -FilePath $LogPath -Append
"env.json present at start: $(Test-Path (Join-Path $RepoDir 'build/env.json'))" | Tee-Object -FilePath $LogPath -Append
"corpus dirs at start:" | Tee-Object -FilePath $LogPath -Append
Get-ChildItem (Join-Path $RepoDir 'tests/fuzz/corpus') -ErrorAction SilentlyContinue |
    ForEach-Object { "  $($_.Name): $((Get-ChildItem $_.FullName -Recurse -File | Measure-Object).Count) files" } |
    Tee-Object -FilePath $LogPath -Append

Step "setup" { python bt.py setup } -Fatal $true
Step "vendor" { python scripts/vendor.py } -Fatal $true
Step "fmt" { python bt.py fmt }
Step "lint" { python bt.py lint }
Step "policy" { python bt.py policy }
Step "check" { python bt.py check }
Step "build" { python bt.py build }
foreach ($area in "common", "core", "scheme", "data", "puzzles", "ga", "tools", "e2e") {
    Step "test-unit-$area" { python bt.py test-unit $area }
}
Step "python-suite" { python -m unittest discover -s tests/python }

"`n===== OVERALL exit=$overall =====" | Tee-Object -FilePath $LogPath -Append
exit $overall
