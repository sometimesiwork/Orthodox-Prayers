# Tests dragging the scrollbar thumb with the mouse. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class B {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  public const uint LEFTDOWN = 2, LEFTUP = 4;
}
"@

Get-Process OrthodoxPrayers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
Remove-Item "$env:APPDATA\OrthodoxPrayers\settings.ini" -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 2300
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[B]::ShowWindow($hwnd, 3) | Out-Null
[B]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 700

$cr = New-Object B+RECT
[B]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$cw = $cr.Right

function Shot([string]$file) {
    $r = New-Object B+RECT
    [B]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path (Resolve-Path "build").Path $file), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "  saved build\$file"
}

function Click([int]$x, [int]$y) {
    $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
    [B]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp) | Out-Null
    [B]::PostMessage($hwnd, 0x0202, [IntPtr]0, $lp) | Out-Null
    Start-Sleep -Milliseconds 300
}

# открываем Псалтирь (длинный раздел)
Click 120 233
Start-Sleep -Milliseconds 600
Shot "ui_21_scroll_before.png"

# тянем ползунок вниз
$screenX = $cw - 7
[B]::SetCursorPos($screenX, 160) | Out-Null
Start-Sleep -Milliseconds 200
[B]::mouse_event([B]::LEFTDOWN, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 200
for ($y = 160; $y -le 620; $y += 40) {
    [B]::SetCursorPos($screenX, $y) | Out-Null
    Start-Sleep -Milliseconds 60
}
Start-Sleep -Milliseconds 300
[B]::mouse_event([B]::LEFTUP, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 700
Shot "ui_22_scroll_after.png"

$proc.CloseMainWindow() | Out-Null
Start-Sleep -Seconds 2
Write-Host "--- scroll position ---"
Get-Content "$env:APPDATA\OrthodoxPrayers\settings.ini" -ErrorAction SilentlyContinue | Select-String "scroll"
