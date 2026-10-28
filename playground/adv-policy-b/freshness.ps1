$here = $PSScriptRoot
$repo = Split-Path -Parent (Split-Path -Parent $here)
$log = Join-Path $here 'freshness.log'
$lines = @()
$lines += 'POLICY_SOURCES'
$lines += Get-ChildItem (Join-Path $repo 'tools/policy') -Include *.c,*.h -Recurse -File | ForEach-Object { $_.Name + ' ' + $_.LastWriteTime.ToString('s') }
$lines += 'POLICY_EXE'
$p = Get-Item (Join-Path $repo 'build/bin/policy.exe')
$lines += $p.LastWriteTime.ToString('s')
$lines += 'VERIFY_SOURCES'
$lines += Get-ChildItem (Join-Path $repo 'tools/verify') -Include *.c,*.h -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 5 | ForEach-Object { $_.Name + ' ' + $_.LastWriteTime.ToString('s') }
$lines += 'VERIFY_EXE'
$v = Get-Item (Join-Path $repo 'build/bin/verify.exe') -ErrorAction SilentlyContinue
if ($v) { $lines += $v.LastWriteTime.ToString('s') } else { $lines += 'MISSING' }
$lines += 'MEMOFREQ_GA_EXES'
foreach ($n in @('memofreq.exe','ga-run.exe')) {
    $e = Get-Item (Join-Path $repo ('build/bin/' + $n)) -ErrorAction SilentlyContinue
    if ($e) { $lines += $n + ' ' + $e.LastWriteTime.ToString('s') } else { $lines += $n + ' MISSING' }
}
$lines += 'MUTATE_EXE_UNTOUCHED_CHECK'
$m = Get-Item (Join-Path $repo 'build/bin/mutate.exe') -ErrorAction SilentlyContinue
if ($m) { $lines += $m.LastWriteTime.ToString('s') }
$lines | Set-Content -Path $log
Get-Content $log
