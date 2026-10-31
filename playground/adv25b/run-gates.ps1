param(
    [Parameter(Mandatory = $true)][string]$RepoDir,
    [Parameter(Mandatory = $true)][string]$LogPath
)

$ErrorActionPreference = "Continue"
$env:Path = "C:\Program Files\LLVM\bin;" + $env:Path
$overall = 0

function Step([string]$Name, [scriptblock]$Body) {
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
    if ($code -ne 0 -and $script:overall -eq 0) { $script:overall = $code }
}

"adv25b real-tree gates" | Tee-Object -FilePath $LogPath
"head: $(git -C $RepoDir rev-parse HEAD)" | Tee-Object -FilePath $LogPath -Append

Step "fmt" { python bt.py fmt }
Step "lint" { python bt.py lint }
Step "policy" { python bt.py policy }
Step "check" { python bt.py check }
Step "build" { python bt.py build }

"`n===== OVERALL exit=$overall =====" | Tee-Object -FilePath $LogPath -Append
exit $overall
