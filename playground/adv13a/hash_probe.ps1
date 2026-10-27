$ErrorActionPreference = 'Stop'
$root = 'C:/Users/lavantien/dev/github/blind-trainer'
foreach ($dir in @("$root/ref_ground/ga", "$root/data/ga", "$root/artifacts/ga")) {
    if (Test-Path $dir) {
        Get-ChildItem $dir -File | ForEach-Object {
            $h = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLower()
            '{0} {1} {2}' -f $h, $_.Length, $_.FullName.Replace($root, '.')
        }
    }
    else {
        "missing dir $dir"
    }
}
Get-ChildItem $root -Recurse -Include '*.bak','*rust-era*','*backup*' -File -ErrorAction SilentlyContinue | Select-Object -First 10 | ForEach-Object { $_.FullName.Replace($root, '.') }
Get-ChildItem "$root/playground" -Directory | ForEach-Object { $_.Name }
