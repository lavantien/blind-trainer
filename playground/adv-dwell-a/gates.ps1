$repo = 'C:/Users/lavantien/dev/github/blind-trainer'
Set-Location $repo
python bt.py fmt *> playground/adv-dwell-a/fmt.log
"fmt_exit $LASTEXITCODE"
Get-Content playground/adv-dwell-a/fmt.log -Tail 25
python bt.py test-unit common *> playground/adv-dwell-a/test-unit-common.log
"common_exit $LASTEXITCODE"
Get-Content playground/adv-dwell-a/test-unit-common.log -Tail 6
