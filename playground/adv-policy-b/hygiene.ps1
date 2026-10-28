$here = $PSScriptRoot
$repo = Split-Path -Parent (Split-Path -Parent $here)
$log = Join-Path $here 'hygiene.log'
$lines = @()
foreach ($c in @('c1e2557','3959aac','afbd7eb','ddb6eed')) {
    $lines += ('COMMIT ' + $c)
    $lines += (git -C $repo show --format='%an <%ae>%n%B' -s $c)
    $lines += 'ADDED_COMMENT_LINES'
    $added = git -C $repo show -U0 --format= $c | Select-String -Pattern '^\+.*(//|/\*)'
    if ($added) { $lines += $added } else { $lines += 'NONE' }
    $lines += 'TRAILERS'
    $body = git -C $repo show -s --format=%B $c
    if ($body -match 'Co-Authored-By|Generated with') { $lines += 'ATTRIBUTION_FOUND' } else { $lines += 'CLEAN' }
    $lines += 'FILES'
    $lines += (git -C $repo show --name-only --format= $c) -join ' '
    $lines += ''
}
$lines | Set-Content -Path $log
Get-Content $log
