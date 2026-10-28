$repo = 'C:/Users/lavantien/dev/github/blind-trainer'
Set-Location $repo
git show df82fee^:ref_ground/ga/report.json > playground/adv-dwell-a/golden_pre_commit.json
"== pre-change snapshot vs old committed golden =="
Compare-Object (Get-Content playground/adv-dwell-a/golden_pre_commit.json) (Get-Content playground/l45/report_pre_change.json) | ForEach-Object { "{0} {1}" -f $_.SideIndicator, $_.InputObject }
"== fresh regen vs pre-change snapshot =="
Compare-Object (Get-Content playground/l45/report_pre_change.json) (Get-Content data/ga/report.json) | ForEach-Object { "{0} {1}" -f $_.SideIndicator, $_.InputObject }
"== capture traces in fresh regen =="
Select-String -Path data/ga/report.json -Pattern 'capture|dwell' | ForEach-Object { $_.Line }
"none above means clean"
