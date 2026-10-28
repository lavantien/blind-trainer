$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot\..\..

foreach ($name in "gendata_scoring_test.c", "gendata_when_test.c") {
    $path = "tests/unit/tools/$name"
    $text = Get-Content $path -Raw
    $text = $text -replace "([0-9a-f])ull\b", '$1ULL'
    Set-Content $path -Value $text -NoNewline
}
Write-Output "lint fixes applied"
