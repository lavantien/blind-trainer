param([string]$Mode = 'constants')
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $root 'build/bin/policy.exe'
$out = & $exe $Mode 2>&1
$code = $LASTEXITCODE
$log = Join-Path $PSScriptRoot ('scan_' + $Mode + '.log')
$out | Set-Content -Path $log
Write-Output ('mode=' + $Mode + ' exit=' + $code + ' lines=' + ($out | Measure-Object).Count)
exit $code
