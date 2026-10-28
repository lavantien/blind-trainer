$log = 'C:/Users/lavantien/dev/github/blind-trainer/playground/adv-dwell-a/fmt.log'
$hits = Select-String -Path $log -Pattern 'error: code should be clang-formatted'
$files = $hits | ForEach-Object { ($_.Line -split ':error')[0] } | Sort-Object -Unique
$files | ForEach-Object { $_ }
"violation_files_count $($files.Count)"
