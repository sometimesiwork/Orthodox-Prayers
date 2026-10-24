# Real-mouse test: open date popup, click a specific day cell, screenshot.
param([int]$Row = 2, [int]$Col = 2)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DP6 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowExW(IntPtr p, IntPtr a, string cls, string t);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern bool GetWindowTextW(IntPtr h, System.Text.StringBuilder sb, int max);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
function Click([int]$x, [int]$y) {
    [DP6]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 120
    [DP6]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 70
    [DP6]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
}
function Shot([IntPtr]$hwnd, [string]$file) {
    $r = New-Object DP6+RECT
    [DP6]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $dc = $g.GetHdc()
    [DP6]::PrintWindow($hwnd, $dc, 2) | Out-Null
    $g.ReleaseHdc($dc)
    $bmp.Save((Join-Path (Resolve-Path (Split-Path $file -Parent)).Path (Split-Path $file -Leaf)), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "saved: $file"
}
$proc = Start-Process -FilePath ".\build\OrthodoxPrayers.exe" -PassThru
Start-Sleep -Milliseconds 2600
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
if ($hwnd -eq [IntPtr]::Zero) { Write-Host "no window"; Stop-Process -Id $proc.Id -Force; exit 1 }
$cr = New-Object DP6+RECT
[DP6]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$pt = New-Object DP6+POINT
$pt.X = $cr.Right - 113; $pt.Y = 66
[DP6]::ClientToScreen($hwnd, [ref]$pt) | Out-Null
Click $pt.X $pt.Y
Start-Sleep -Milliseconds 700
$popup = [DP6]::FindWindowExW([IntPtr]::Zero, [IntPtr]::Zero, "OPDatePopup", $null)
if ($popup -eq [IntPtr]::Zero) { Write-Host "popup NOT found"; Stop-Process -Id $proc.Id -Force; exit 1 }
$pr = New-Object DP6+RECT
[DP6]::GetWindowRect($popup, [ref]$pr) | Out-Null
$cellX = $pr.Left + 14 + $Col * 33 + 16
$cellY = $pr.Top + 56 + $Row * 30 + 15
Write-Host ("click cell row={0} col={1} at screen ({2},{3})" -f $Row, $Col, $cellX, $cellY)
Click $cellX $cellY
Start-Sleep -Milliseconds 900
$popup2 = [DP6]::FindWindowExW([IntPtr]::Zero, [IntPtr]::Zero, "OPDatePopup", $null)
Write-Host ($(if ($popup2 -eq [IntPtr]::Zero) { "popup closed (date picked)" } else { "popup STILL open" }))
$sb = New-Object System.Text.StringBuilder 256
[DP6]::GetWindowTextW($hwnd, $sb, 256) | Out-Null
Write-Host ("window title: " + $sb.ToString())
Shot $hwnd ".\build\ui_datepick_real.png"
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
