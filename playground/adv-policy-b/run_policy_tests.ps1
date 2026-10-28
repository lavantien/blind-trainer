$here = $PSScriptRoot
$repo = Split-Path -Parent (Split-Path -Parent $here)
$log = Join-Path $here 'policy_tests.log'
Push-Location $repo
python bt.py test-unit tools --only policy *> $log
$code = $LASTEXITCODE
Pop-Location
Write-Output ("policy unit exit=" + $code)
Get-Content $log | Select-String -Pattern 'passed|failed|PASS|FAIL|test' | Select-Object -Last 30
