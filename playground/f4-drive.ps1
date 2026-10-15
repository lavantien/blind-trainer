param(
    [string]$Exe = "target/release/blind-trainer.exe",
    [string]$OutDir = "artifacts/f4-drive",
    [int]$HoldSecs = 70
)
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$log = Join-Path $OutDir "stderr.log"
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class NativeInput {
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, int data, UIntPtr extra);
    public const uint LEFTDOWN = 0x0002;
    public const uint LEFTUP = 0x0004;
    public const uint WHEEL = 0x0800;
}
"@
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class NativeWin {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindow(IntPtr zero, string title);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
}
"@
function Window-Rect([string]$title) {
    $deadline = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $deadline) {
        $rect = New-Object NativeWin+RECT
        $hwnd = [NativeWin]::FindWindow([IntPtr]::Zero, $title)
        if ($hwnd -ne [IntPtr]::Zero -and [NativeWin]::GetWindowRect($hwnd, [ref]$rect)) {
            if (($rect.Right - $rect.Left) -gt 100) { return $rect }
        }
        Start-Sleep -Milliseconds 500
    }
    throw "window not found: $title"
}
function Shoot([string]$name) {
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $gfx = [System.Drawing.Graphics]::FromImage($bmp)
    $gfx.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $bmp.Save((Join-Path $OutDir "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $gfx.Dispose(); $bmp.Dispose()
    Write-Host "shot $name"
}
$shell = New-Object -ComObject WScript.Shell
function Tap([string]$keys, [int]$pauseMs = 700) {
    $ok = $false
    for ($i = 0; $i -lt 5 -and -not $ok; $i++) {
        $ok = $shell.AppActivate($proc.Id)
        if (-not $ok) {
            $ok = [NativeWin]::SetForegroundWindow([NativeWin]::FindWindow([IntPtr]::Zero, "blind trainer"))
        }
        Start-Sleep -Milliseconds 150
    }
    if (-not $ok) { throw "cannot activate window for keys $keys" }
    try { $shell.SendKeys($keys) } catch { Start-Sleep -Milliseconds 300; $shell.SendKeys($keys) }
    Start-Sleep -Milliseconds $pauseMs
}
function Drag([int]$dxTotal, [int]$dyTotal, [int]$steps = 20) {
    $rect = Window-Rect "blind trainer"
    $cx = [int](($rect.Left + $rect.Right) / 2)
    $cy = [int](($rect.Top + $rect.Bottom) / 2)
    [void]$shell.AppActivate($proc.Id)
    [void][NativeInput]::SetCursorPos($cx, $cy)
    Start-Sleep -Milliseconds 150
    [NativeInput]::mouse_event([NativeInput]::LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
    for ($i = 1; $i -le $steps; $i++) {
        $x = $cx + [int]($dxTotal * $i / $steps)
        $y = $cy + [int]($dyTotal * $i / $steps)
        [void][NativeInput]::SetCursorPos($x, $y)
        Start-Sleep -Milliseconds 30
    }
    [NativeInput]::mouse_event([NativeInput]::LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 500
}
function Wheel([int]$notches) {
    [void]$shell.AppActivate($proc.Id)
    for ($i = 0; $i -lt [Math]::Abs($notches); $i++) {
        $data = [int]([Math]::Sign($notches) * 120)
        [NativeInput]::mouse_event([NativeInput]::WHEEL, 0, 0, $data, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 40
    }
    Start-Sleep -Milliseconds 600
}
$proc = Start-Process -FilePath $Exe -WorkingDirectory (Get-Location) -RedirectStandardError $log -PassThru
try {
    Start-Sleep -Seconds 6
    [void](Window-Rect "blind trainer")
    Shoot "01-menu"
    Tap "3" 1500
    Shoot "02-solve-ready"
    Tap " " 800
    Tap " " 800
    Tap " " 1200
    Shoot "03-solve-review"
    Tap "v" 1500
    Shoot "04-replay-paused"
    Tap " " 1300
    Shoot "05-replay-mid-animation"
    Drag 420 0
    Shoot "06-drag-yaw"
    Drag 0 -700
    Shoot "07-drag-pitch-clamp"
    Wheel 25
    Shoot "08-zoom-min-bound"
    Wheel -60
    Shoot "09-zoom-max-bound"
    Tap "r" 900
    Shoot "10-orbit-reset"
    Tap "v" 1000
    Tap "v" 1200
    Tap " " 350
    Tap "v" 1500
    Shoot "11-exit-mid-animation-to-solve"
    Start-Sleep -Seconds 2
} finally {
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
    Start-Sleep -Milliseconds 500
}
Get-ChildItem $OutDir | Select-Object Name, Length
Get-Content $log -Tail 20
