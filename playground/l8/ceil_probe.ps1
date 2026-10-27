$exe = "C:\Users\lavantien\dev\github\blind-trainer\build\bin\tools_jobwrap_ceiling_test.exe"
try {
    $p = Start-Process -FilePath $exe -Wait -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\ceil.out" -RedirectStandardError "$env:TEMP\ceil.err"
    Write-Host "exit: $($p.ExitCode)"
} catch {
    Write-Host "exception: $($_.Exception.Message)"
}
Get-Item "$env:TEMP\ceil.out", "$env:TEMP\ceil.err" | ForEach-Object { Write-Host "$($_.Name) $($_.Length)" }
