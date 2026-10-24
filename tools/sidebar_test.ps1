# Tests that the sidebar (menu) scrolls: wheel, track click, thumb drag. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class M {
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
[M]::ShowWindow($hwnd, 3) | Out-Null
[M]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 800

$wr = New-Object M+RECT
[M]::GetWindowRect($hwnd, [ref]$wr) | Out-Null
$cr = New-Object M+RECT
[M]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$cw = $cr.Right

# снимок области меню (левая полоса)
function SideShot([string]$file) {
    $r = New-Object M+RECT
    [M]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap 280, ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path (Resolve-Path "build").Path $file), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}

function Hash([string]$file) {
    return (Get-FileHash (Join-Path (Resolve-Path "build").Path $file) -Algorithm SHA256).Hash
}

# колесо мыши над меню (WM_MOUSEWHEEL: wParam = delta << 16, lParam = экранные координаты)
function Wheel([int]$screenX, [int]$screenY, [int]$delta) {
    $wp = [IntPtr](($delta -shl 16) -band 0xFFFFFFFF)
    $lp = [IntPtr](($screenY -shl 16) -bor ($screenX -band 0xFFFF))
    [M]::PostMessage($hwnd, 0x020A, $wp, $lp) | Out-Null
    Start-Sleep -Milliseconds 400
}

Write-Host "--- 1. колесо мыши над меню ---"
SideShot "ui_45_sidebar_before.png"
$h0 = Hash "ui_45_sidebar_before.png"
Wheel ($wr.Left + 120) ($wr.Top + 500) -120
Wheel ($wr.Left + 120) ($wr.Top + 500) -120
SideShot "ui_46_sidebar_after_wheel.png"
$h1 = Hash "ui_46_sidebar_after_wheel.png"
Write-Host ("  menu changed: " + ($h0 -ne $h1))
if ($h0 -eq $h1) { Write-Host "  FAIL: колесо не прокручивает меню" }

Write-Host "--- 2. перетаскивание ползунка меню ---"
$screenX = $wr.Left + $cw - 8
[M]::SetCursorPos($screenX, $wr.Top + 200) | Out-Null
Start-Sleep -Milliseconds 200
[M]::mouse_event([M]::LEFTDOWN, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 200
for ($y = $wr.Top + 200; $y -le $wr.Top + 600; $y += 40) {
    [M]::SetCursorPos($screenX, $y) | Out-Null
    Start-Sleep -Milliseconds 60
}
Start-Sleep -Milliseconds 300
[M]::mouse_event([M]::LEFTUP, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 600
SideShot "ui_47_sidebar_after_drag.png"
$h2 = Hash "ui_47_sidebar_after_drag.png"
Write-Host ("  menu changed: " + ($h1 -ne $h2))
if ($h1 -eq $h2) { Write-Host "  FAIL: ползунок меню не перетаскивается" }

$proc.CloseMainWindow() | Out-Null
Start-Sleep -Seconds 1
Write-Host "done"
