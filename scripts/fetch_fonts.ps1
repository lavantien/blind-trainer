$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$fontsDir = Join-Path $root 'assets/fonts'
New-Item -ItemType Directory -Force -Path $fontsDir | Out-Null
$ua = 'blind-trainer-fonts-fetch/1.0 (pinned asset download)'

$files = @(
    [pscustomobject]@{ Name = 'NotoSans-Regular.ttf'; Sha256 = '478c558ea716033cd60c03438f628dfa75694dcf6b5f6d505a2f05fd2b4f3823'; Url = 'https://raw.githubusercontent.com/notofonts/notofonts.github.io/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf' }
    [pscustomobject]@{ Name = 'NotoSans-Bold.ttf'; Sha256 = '1df075a380fc7cb898acf64c1f7b3b4dd780de3caa860178bf929de35817a913'; Url = 'https://raw.githubusercontent.com/notofonts/notofonts.github.io/main/fonts/NotoSans/hinted/ttf/NotoSans-Bold.ttf' }
    [pscustomobject]@{ Name = 'OFL.txt'; Sha256 = 'f2095b08bed08b23a6fe26112fcd679a2bee3f002eef077eb05d215ed1051bd8'; Url = 'https://raw.githubusercontent.com/notofonts/notofonts.github.io/main/fonts/LICENSE' }
)

$failed = $false
foreach ($file in $files) {
    $target = Join-Path $fontsDir $file.Name
    $needDownload = -not (Test-Path $target)
    if (-not $needDownload) {
        $current = (Get-FileHash -Algorithm SHA256 $target).Hash.ToLowerInvariant()
        if ($current -ne $file.Sha256) {
            Write-Host "hash mismatch for existing $($file.Name): $current, refetching"
            $needDownload = $true
        }
    }
    if ($needDownload) {
        $tmp = "$target.download"
        & curl.exe --fail --location --silent --show-error --retry 3 --retry-delay 2 -A $ua -o $tmp $file.Url
        if ($LASTEXITCODE -ne 0) {
            Write-Host "download failed for $($file.Name) from $($file.Url)"
            $failed = $true
            continue
        }
        Move-Item -Force $tmp $target
    }
    $actual = (Get-FileHash -Algorithm SHA256 $target).Hash.ToLowerInvariant()
    if ($actual -ne $file.Sha256) {
        Write-Host "hash mismatch for $($file.Name): expected $($file.Sha256), got $actual"
        $failed = $true
    } else {
        Write-Host "ok $($file.Name) $actual"
    }
}

if ($failed) {
    Write-Error 'font fetch failed: hash mismatch or download error'
    exit 1
}
