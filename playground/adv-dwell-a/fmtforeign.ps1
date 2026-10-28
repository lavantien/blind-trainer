$repo = 'C:/Users/lavantien/dev/github/blind-trainer'
Set-Location $repo
$cf = (Get-Command clang-format).Source
& $cf --dry-run --Werror src/app/boot.c src/app/smoke.c src/app/usage.c src/app/views/fatal.c *> playground/adv-dwell-a/fmt-foreign.log
"fmt_foreign_exit $LASTEXITCODE"
Get-Content playground/adv-dwell-a/fmt-foreign.log | Select-Object -First 6
