$here = $PSScriptRoot
$repo = Split-Path -Parent (Split-Path -Parent $here)
$log = Join-Path $here 'units.log'
$lines = @()
Push-Location $repo
python bt.py test-unit app *>> $log
$lines += ('app_exit=' + $LASTEXITCODE)
python bt.py test-unit tools --only verify *>> $log
$lines += ('verify_exit=' + $LASTEXITCODE)
Pop-Location
$lines += (Get-Content $log | Select-String -Pattern 'summary|fail' | Select-Object -Last 8)
$lines | Set-Content -Path (Join-Path $here 'units_summary.log')
Get-Content (Join-Path $here 'units_summary.log')
