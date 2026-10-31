param(
    [Parameter(Mandatory = $true)][string]$CloneDir,
    [Parameter(Mandatory = $true)][string]$LogFile
)

function Log([string]$text) {
    $stamp = Get-Date -Format "yyyy-MM-ddTHH:mm:ss"
    Add-Content -Path $LogFile -Value "$stamp $text"
}

function RunStep([string]$name, [scriptblock]$body) {
    Log "step-entry $name"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & $body
    $code = $LASTEXITCODE
    $sw.Stop()
    Log "step-exit $name code=$code dt=$([int]$sw.Elapsed.TotalSeconds)s"
}

Push-Location $CloneDir
try {
    RunStep "test-unit-tools" { python bt.py test-unit tools *>> $LogFile }
    RunStep "test-unit-e2e" { python bt.py test-unit e2e *>> $LogFile }
    RunStep "python-suite" { python -m unittest discover -s tests/python *>> $LogFile }
    Log "exit clone-rest done"
    Pop-Location
    exit 0
}
catch {
    Log "fail unexpected: $($_.Exception.Message)"
    Pop-Location
    exit 99
}
