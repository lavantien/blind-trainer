$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..\..')
$repo = (Get-Location).Path
$exe = Join-Path $repo 'build/bin/bt-app.exe'
$scratch = Join-Path $repo 'playground/l49/determinism'
if (Test-Path $scratch) {
    Remove-Item -Recurse -Force $scratch
}
$env:BT_REF_GROUND = 'ref_ground'
$env:BT_CONFIG = Join-Path $repo 'config/config.json'
$env:BT_ARTIFACTS = 'artifacts'

foreach ($run in @('run1', 'run2')) {
    $dir = Join-Path $scratch $run
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $env:APPDATA = Join-Path $scratch "user-$run"
    Write-Output "[determinism] usage tour into $run"
    & $exe --usage $dir
    Write-Output "[determinism] $run exit: $LASTEXITCODE"
}

$one = Get-ChildItem (Join-Path $scratch 'run1') -Filter *.png
$diffCount = 0
$sameCount = 0
foreach ($file in $one) {
    $other = Join-Path (Join-Path $scratch 'run2') $file.Name
    if (-not (Test-Path $other)) {
        Write-Output ("[determinism] MISSING in run2: " + $file.Name)
        $diffCount++
        continue
    }
    $a = Get-FileHash $file.FullName -Algorithm SHA256
    $b = Get-FileHash $other -Algorithm SHA256
    if ($a.Hash -eq $b.Hash) {
        $sameCount++
    } else {
        Write-Output ("[determinism] DIFFER: " + $file.Name)
        $diffCount++
    }
}
Write-Output ("[determinism] identical: $sameCount  differing: $diffCount  of " + $one.Count)
if ($diffCount -eq 0) {
    Write-Output '[determinism] VERDICT: PASS (byte-identical tours)'
} else {
    Write-Output '[determinism] VERDICT: FAIL (nondeterministic shots)'
}
exit 0
