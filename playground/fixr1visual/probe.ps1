param([string]$Scenario = 'all', [string]$ReportDir = "$PSScriptRoot/out")

$cases = @{
    clean      = @{ first = 0; smoke = 0; second = 0; wantExit = 0; wantVerdict = $null }
    flake      = @{ first = 1; smoke = 0; second = 0; wantExit = 3; wantVerdict = 'infra flake, second run passed' }
    regression = @{ first = 1; smoke = 0; second = 1; wantExit = 2; wantVerdict = 'regression, both runs mismatched' }
    infraerror = @{ first = 1; smoke = 5; second = 0; wantExit = 4; wantVerdict = 'infra error, re-render failed' }
}

if ($Scenario -ne 'all') {
    $case = $cases[$Scenario]
    $report = Join-Path $ReportDir "$Scenario-report.md"
    $diffCalls = 0
    function make([string]$target) {
        if ($target -eq 'visual-diff') {
            $script:diffCalls++
            $code = if ($script:diffCalls -eq 1) { $case.first } else { $case.second }
        } elseif ($target -eq 'e2e-smoke') {
            $code = $case.smoke
        } else {
            $code = 0
        }
        pwsh -NoProfile -Command "exit $code"
    }
    make visual-diff; if ($LASTEXITCODE -eq 0) { exit 0 }; Write-Output 'visual: mismatch, re-rendering and re-diffing once'; make e2e-smoke; if ($LASTEXITCODE -ne 0) { Add-Content -Path $report -Value "`nverdict: infra error, re-render failed"; exit 4 }; make visual-diff; if ($LASTEXITCODE -eq 0) { Add-Content -Path $report -Value "`nverdict: infra flake, second run passed"; exit 3 } else { Add-Content -Path $report -Value "`nverdict: regression, both runs mismatched"; exit 2 }
    exit 99
}

New-Item -ItemType Directory -Path $ReportDir -Force | Out-Null
$failed = $false
foreach ($name in @($cases.Keys)) {
    $report = Join-Path $ReportDir "$name-report.md"
    Remove-Item $report -Force -ErrorAction SilentlyContinue
    pwsh -NoProfile -File $PSCommandPath -Scenario $name -ReportDir $ReportDir | Out-Null
    $code = $LASTEXITCODE
    $verdict = ''
    if (Test-Path $report) { $verdict = ((Get-Content $report -Raw).Trim() -replace '^\s*verdict:\s*', '') }
    $want = $cases[$name]
    $ok = ($code -eq $want.wantExit) -and ($verdict -eq [string]$want.wantVerdict)
    if (-not $ok) { $failed = $true }
    '{0}: exit {1} verdict "{2}" want-exit {3} want-verdict "{4}" {5}' -f $name, $code, $verdict, $want.wantExit, $want.wantVerdict, ($(if ($ok) { 'ok' } else { 'FAIL' }))
}
if ($failed) { exit 1 }
exit 0
