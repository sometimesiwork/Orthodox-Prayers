# Tests tooltips, collapsed sidebar and the back-to-top button. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class I {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
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
[I]::ShowWindow($hwnd, 3) | Out-Null
[I]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 700

$cr = New-Object I+RECT
[I]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$cw = $cr.Right

function Shot([string]$file) {
    $r = New-Object I+RECT
    [I]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path (Resolve-Path "build").Path $file), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "  saved build\$file"
}

function Hover([int]$x, [int]$y) {
    $pt = New-Object I+POINT
    $pt.X = $x
    $pt.Y = $y
    [I]::ClientToScreen($hwnd, [ref]$pt) | Out-Null
    [I]::SetCursorPos($pt.X, $pt.Y) | Out-Null
    for ($i = 0; $i -lt 8; $i++) {
        [I]::SetCursorPos($pt.X + $i, $pt.Y) | Out-Null
        Start-Sleep -Milliseconds 90
    }
    Start-Sleep -Milliseconds 800
}

# 1) подсказка над кнопкой «Содержание»
Hover ($cw - 126) 24
Shot "ui_23_tooltip.png"

# 2) сворачивание боковой панели
$lp = [IntPtr]((24 -shl 16) -bor (240 -band 0xFFFF))
[I]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp) | Out-Null
[I]::PostMessage($hwnd, 0x0202, [IntPtr]0, $lp) | Out-Null
Start-Sleep -Milliseconds 600
Shot "ui_24_collapsed.png"

# 3) кнопка «вверх» после прокрутки
for ($i = 0; $i -lt 10; $i++) {
    $w = [IntPtr]([int](-120) -shl 16)
    [I]::PostMessage($hwnd, 0x020A, $w, [IntPtr]1000) | Out-Null
    Start-Sleep -Milliseconds 80
}
Start-Sleep -Milliseconds 700
Shot "ui_25_backtotop.png"

$proc.CloseMainWindow() | Out-Null
Start-Sleep -Seconds 1
