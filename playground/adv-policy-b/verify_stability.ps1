$here = $PSScriptRoot
$repo = Split-Path -Parent (Split-Path -Parent $here)
$log = Join-Path $here 'verify_stability.log'
$lines = @()
$env:BT_REF_GROUND = 'ref_ground'
$env:BT_CONFIG = (Join-Path $repo 'config/config.json')
$env:BT_ARTIFACTS = 'artifacts'
$dir = Join-Path $repo 'artifacts/verify'
$a = Join-Path $here 'report_a.md'
$b = Join-Path $here 'report_b.md'
$ja = Join-Path $here 'report_a.json'
$jb = Join-Path $here 'report_b.json'
Copy-Item (Join-Path $dir 'report.md') $a -Force
Copy-Item (Join-Path $dir 'report.json') $ja -Force
Push-Location $repo
$v = & (Join-Path $repo 'build/bin/verify.exe') 2>&1
$lines += $v
$lines += ('exit=' + $LASTEXITCODE)
Pop-Location
Copy-Item (Join-Path $dir 'report.md') $b -Force
Copy-Item (Join-Path $dir 'report.json') $jb -Force
$lines += 'REPORT_MD_DIFF'
$lines += (Compare-Object (Get-Content $a) (Get-Content $b) | ForEach-Object { $_.SideIndicator + ' ' + $_.InputObject })
$lines += 'REPORT_JSON_DIFF'
$jaText = Get-Content $ja -Raw
$jbText = Get-Content $jb -Raw
$lines += ('json_equal_except_stamp=' + (($jaText -replace '"stamp":"[^"]*"','"stamp":X') -eq ($jbText -replace '"stamp":"[^"]*"','"stamp":X')))
$lines += 'SHEET_ORDER'
$lines += (Get-Content $b | Select-String -Pattern '^\| (cherry|eggins) ' | ForEach-Object { $_.Line })
$lines | Set-Content -Path $log
Get-Content $log
