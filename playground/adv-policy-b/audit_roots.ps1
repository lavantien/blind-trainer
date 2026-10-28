$here = $PSScriptRoot
$repo = Split-Path -Parent (Split-Path -Parent $here)
$log = Join-Path $here 'roots_audit.log'
$lines = @()
$lines += 'TOPLEVEL_DIRS'
$lines += (Get-ChildItem $repo -Directory | ForEach-Object { $_.Name }) -join ' '
$lines += 'STRAY_SOURCE_ROOTS'
$stray = Get-ChildItem $repo -Recurse -Include *.c,*.h,*.py -File | Where-Object {
    $rel = $_.FullName.Substring($repo.Length + 1)
    $parts = $rel -split '[\\/]'
    $top = $parts[0]
    $skipped = $parts | Where-Object { $_ -in @('vendor','build','crates','target','ref_ground','ref_doc','playground','artifacts','__pycache__') }
    ($top -notin @('src','tools','tests','scripts')) -and ($null -eq $skipped -or $skipped.Count -eq 0)
}
$lines += $stray | ForEach-Object { $_.FullName.Substring($repo.Length + 1) }
$lines += 'COUNTS'
$inScope = Get-ChildItem (Join-Path $repo 'src'),(Join-Path $repo 'tools'),(Join-Path $repo 'tests'),(Join-Path $repo 'scripts') -Recurse -Include *.c,*.h,*.py -File | Where-Object {
    $rel = $_.FullName.Substring($repo.Length + 1)
    $parts = $rel -split '[\\/]'
    -not ($parts | Where-Object { $_ -in @('vendor','build','crates','target','ref_ground','ref_doc','playground','artifacts','__pycache__') })
}
$lines += ('in_scope_total=' + $inScope.Count)
$lines += ('constants_scope=' + (($inScope | Where-Object { -not ($_.FullName.Substring($repo.Length + 1) -split '[\\/]' -contains 'tests') }).Count))
$lines | Set-Content -Path $log
Get-Content $log
