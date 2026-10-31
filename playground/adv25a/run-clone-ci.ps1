param(
    [Parameter(Mandatory = $true)][string]$CloneDir,
    [Parameter(Mandatory = $true)][string]$LogFile
)

$ErrorActionPreference = "Continue"
$repo = "C:\Users\lavantien\dev\github\blind-trainer"

function Log([string]$text) {
    $stamp = Get-Date -Format "yyyy-MM-ddTHH:mm:ss"
    Add-Content -Path $LogFile -Value "$stamp $text"
}

Log "entry clone-ci clone=$CloneDir python=$(python --version 2>&1)"

if (Test-Path $CloneDir) {
    Log "fail clone exists already: $CloneDir"
    exit 9
}

git clone --no-hardlinks $repo $CloneDir *> $null
if ($LASTEXITCODE -ne 0) {
    Log "fail git clone exit $LASTEXITCODE"
    exit 8
}
Log "clone done head=$(git -C $CloneDir rev-parse --short HEAD)"

$freshState = @("build", "vendor", "data\vendor", "data\ga")
foreach ($rel in $freshState) {
    if (Test-Path (Join-Path $CloneDir $rel)) {
        Log "fail fresh-clone not fresh: $rel present"
        exit 7
    }
}
Log "fresh-clone verified no build vendor data/vendor data/ga"

function RunStep([string]$name, [scriptblock]$body) {
    Log "step-entry $name"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & $body
    $code = $LASTEXITCODE
    $sw.Stop()
    Log "step-exit $name code=$code dt=$([int]$sw.Elapsed.TotalSeconds)s"
    if ($code -ne 0) {
        Log "fail chain aborted at $name"
        exit $code
    }
}

Push-Location $CloneDir
try {
    RunStep "setup" { python bt.py setup *>> $LogFile }
    RunStep "vendor" { python scripts/vendor.py *>> $LogFile }
    RunStep "fmt" { python bt.py fmt *>> $LogFile }
    RunStep "lint" { python bt.py lint *>> $LogFile }
    RunStep "policy" { python bt.py policy *>> $LogFile }
    RunStep "check" { python bt.py check *>> $LogFile }
    RunStep "build" { python bt.py build *>> $LogFile }
    foreach ($area in "common", "core", "scheme", "data", "puzzles", "ga", "tools", "e2e") {
        RunStep "test-unit-$area" { python bt.py test-unit $area *>> $LogFile }
    }
    RunStep "python-suite" { python -m unittest discover -s tests/python *>> $LogFile }
    Log "exit clone-ci full chain green"
    Pop-Location
    exit 0
}
catch {
    Log "fail unexpected: $($_.Exception.Message)"
    Pop-Location
    exit 99
}
