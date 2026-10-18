$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$makefile = Join-Path $root 'Makefile'
if (-not (Test-Path $makefile)) { throw "missing Makefile at $makefile" }
Write-Output 'Usage: make <target>'
Write-Output ''
Write-Output 'Targets:'
$rows = @()
Get-Content $makefile | ForEach-Object {
    if ($_ -match '^([a-zA-Z0-9_-]+):[^=#]*?##\s*(.+?)\s*$') {
        $rows += ,@($Matches[1], $Matches[2])
    } elseif ($_ -notmatch '^\s' -and $_ -match '##') {
        throw "unparsed target doc line: $_"
    }
}
if ($rows.Count -eq 0) { throw "no ## target docs found in $makefile" }
foreach ($row in $rows) {
    Write-Output ('{0,-24} {1}' -f $row[0], $row[1])
}
