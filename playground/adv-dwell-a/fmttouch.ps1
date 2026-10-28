$repo = 'C:/Users/lavantien/dev/github/blind-trainer'
Set-Location $repo
$cf = (Get-Command clang-format -ErrorAction Stop).Source
"clang-format at $cf"
$files = @(
    'src/app/config.c',
    'src/app/config.h',
    'src/app/window.h',
    'tests/unit/app/config_test.c',
    'tests/unit/app/drivers_test.c',
    'tests/unit/app/ui_draw_test.c',
    'tests/unit/app/view_draw_b_test.c',
    'tests/unit/app/window_test.c'
)
& $cf --dry-run --Werror @files *> playground/adv-dwell-a/fmt-touched.log
"fmt_touched_exit $LASTEXITCODE"
Get-Content playground/adv-dwell-a/fmt-touched.log
