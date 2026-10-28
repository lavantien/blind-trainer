$here = $PSScriptRoot
$repo = Split-Path -Parent (Split-Path -Parent $here)
foreach ($n in @('report_a.md','report_b.md','report_a.json','report_b.json')) {
    Remove-Item (Join-Path $here $n) -Force -ErrorAction SilentlyContinue
}
Remove-Item (Join-Path $here 'units.log') -Force -ErrorAction SilentlyContinue
Push-Location $repo
python scripts/commit_lane.py -m "test(adv-policy-b): scanner rederivation, allowlist liveness, behavior identity probes" playground/adv-policy-b
$code = $LASTEXITCODE
Pop-Location
Write-Output ("commit exit=" + $code)
