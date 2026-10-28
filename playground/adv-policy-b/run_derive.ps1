$here = $PSScriptRoot
$repo = Split-Path -Parent (Split-Path -Parent $here)
$python = "python"
& $python -I (Join-Path $here 'derive.py') > (Join-Path $here 'derive_out.log') 2>&1
$code = $LASTEXITCODE
Write-Output ("derive exit=" + $code)
exit $code
