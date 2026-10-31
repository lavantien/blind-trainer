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

Log "entry l25-clone-ci clone=$CloneDir head=$(git -C $repo rev-parse --short HEAD)"

if (Test-Path $CloneDir) {
    Log "fail clone exists already: $CloneDir"
    exit 9
}

git -c core.autocrlf=true clone --no-hardlinks $repo $CloneDir *> $null
if ($LASTEXITCODE -ne 0) {
    Log "fail git clone exit $LASTEXITCODE"
    exit 8
}
git -C $CloneDir config core.autocrlf true
Log "clone done autocrlf=$(git -C $CloneDir config core.autocrlf)"

$corpusDrift = 0
$corpusTotal = 0
Get-ChildItem "$repo\tests\fuzz\corpus" -Recurse -File | ForEach-Object {
    $corpusTotal++
    $rel = [IO.Path]::GetRelativePath($repo, $_.FullName)
    $twin = Join-Path $CloneDir $rel
    if (-not (Test-Path -LiteralPath $twin)) {
        $corpusDrift++
    } elseif ((Get-FileHash -LiteralPath $_.FullName).Hash -ne (Get-FileHash -LiteralPath $twin).Hash) {
        $corpusDrift++
    }
}
Log "corpus bytes under forced autocrlf: $corpusTotal files, $corpusDrift drift"
if ($corpusDrift -ne 0) {
    Log "fail corpus bytes drifted under autocrlf checkout"
    exit 7
}

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
    RunStep "grounding" { python scripts/fetch_docs.py --only vietnamese/syllables-7184-abc.txt,vietnamese/vn-word-frequencies.tsv *>> $LogFile }
    RunStep "fmt" { python bt.py fmt *>> $LogFile }
    RunStep "lint" { python bt.py lint *>> $LogFile }
    RunStep "policy" { python bt.py policy *>> $LogFile }
    RunStep "check" { python bt.py check *>> $LogFile }
    RunStep "build" { python bt.py build *>> $LogFile }
    foreach ($area in "common", "core", "scheme", "data", "puzzles", "ga", "e2e") {
        RunStep "test-unit-$area" { python bt.py test-unit $area *>> $LogFile }
    }
    RunStep "test-unit-tools-without-ga-run" { python bt.py test-unit tools --without ga_run_lib *>> $LogFile }
    RunStep "python-suite" { python -m unittest discover -s tests/python *>> $LogFile }
    Log "exit l25-clone-ci full chain green"
    Pop-Location
    exit 0
}
catch {
    Log "fail unexpected: $($_.Exception.Message)"
    Pop-Location
    exit 99
}
