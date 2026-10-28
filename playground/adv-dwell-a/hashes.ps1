$repo = 'C:/Users/lavantien/dev/github/blind-trainer'
Set-Location $repo
foreach ($f in 'ref_ground/ga/freq.json', 'ref_ground/ga/syllables.json', 'ref_ground/ga/report.json', 'ref_ground/ga/report.md', 'playground/l45/report_pre_change.json') {
    $h = (Get-FileHash $f -Algorithm SHA256).Hash.ToLower()
    "${f} $h"
}
git check-ignore -v data/ga/report.json
"check_ignore_exit $LASTEXITCODE"
foreach ($e in 'build/bin/memofreq.exe', 'build/bin/ga-run.exe') {
    $i = Get-Item $e
    "{0} len={1} mtime={2}" -f $e, $i.Length, $i.LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss')
}
