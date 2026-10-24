# Real-mouse test: month nav arrows, pick date in another month, footer, dismiss.
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DP7 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowExW(IntPtr p, IntPtr a, string cls, string t);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
function Click([int]$x, [int]$y) {
    [DP7]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 130
    [DP7]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 70
    [DP7]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
}
function Shot([IntPtr]$hwnd, [string]$file) {
    $r = New-Object DP7+RECT
    [DP7]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $dc = $g.GetHdc()
    [DP7]::PrintWindow($hwnd, $dc, 2) | Out-Null
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

function Get-Popup {
    return [DP7]::FindWindowExW([IntPtr]::Zero, [IntPtr]::Zero, "OPDatePopup", $null)
}
function TogglePopup {
    $cr = New-Object DP7+RECT
    [DP7]::GetClientRect($hwnd, [ref]$cr) | Out-Null
    $pt = New-Object DP7+POINT
    $pt.X = $cr.Right - 113; $pt.Y = 66
    [DP7]::ClientToScreen($hwnd, [ref]$pt) | Out-Null
    Click $pt.X $pt.Y
    Start-Sleep -Milliseconds 750
    $p = Get-Popup
    if ($p -eq [IntPtr]::Zero) {
        Start-Sleep -Milliseconds 600
        $p = Get-Popup
    }
    return $p
}

# 1) open popup
$popup = TogglePopup
if ($popup -eq [IntPtr]::Zero) { Write-Host "popup NOT open"; Stop-Process -Id $proc.Id -Force; exit 1 }
$pr = New-Object DP7+RECT
[DP7]::GetWindowRect($popup, [ref]$pr) | Out-Null
Write-Host ("popup at ({0},{1}) {2}x{3}" -f $pr.Left, $pr.Top, ($pr.Right-$pr.Left), ($pr.Bottom-$pr.Top))

# 2) click ">" arrow twice: October -> November -> December
$navX = $pr.Left + 259 - 14 - 30 + 15   # right nav zone center
$navY = $pr.Top + 17
Click $navX $navY
Start-Sleep -Milliseconds 450
Click $navX $navY
Start-Sleep -Milliseconds 550
Shot $popup ".\build\popup_nav_next2.png"
if ((Get-Popup) -ne [IntPtr]::Zero) { Write-Host "nav: popup still open (good)" } else { Write-Host "nav: popup CLOSED (bug)" }

# 3) click "<" twice -> back to October
$navXL = $pr.Left + 14 + 15
Click $navXL $navY
Start-Sleep -Milliseconds 450
Click $navXL $navY
Start-Sleep -Milliseconds 450
# 4) click "<" once more -> September, then pick cell (2,2) = Sept 9
Click $navXL $navY
Start-Sleep -Milliseconds 550
Shot $popup ".\build\popup_nav_back3.png"
$cellX = $pr.Left + 14 + 2 * 33 + 16
$cellY = $pr.Top + 56 + 2 * 30 + 15
Click $cellX $cellY
Start-Sleep -Milliseconds 900
if ((Get-Popup) -eq [IntPtr]::Zero) { Write-Host "picked Sept 9 (popup closed)" } else { Write-Host "popup still open" }

# 5) reopen, click ">" to keep visible, then footer "Сегодня"
$popup = TogglePopup
if ($popup -ne [IntPtr]::Zero) {
    [DP7]::GetWindowRect($popup, [ref]$pr) | Out-Null
    $footX = $pr.Left + 130
    $footY = $pr.Top + 56 + 180 + 14
    Click $footX $footY
    Start-Sleep -Milliseconds 800
    if ((Get-Popup) -eq [IntPtr]::Zero) { Write-Host "footer: popup closed" } else { Write-Host "footer: popup open (bug)" }
}

# 6) open, click far outside -> must close (dismiss)
$popup = TogglePopup
if ($popup -ne [IntPtr]::Zero) {
    Click 60 500
    Start-Sleep -Milliseconds 700
    if ((Get-Popup) -eq [IntPtr]::Zero) { Write-Host "dismiss: popup closed (good)" } else { Write-Host "dismiss: popup OPEN (bug)" }
}

# main-window screenshot to verify final state
Shot $hwnd ".\build\ui_after_nav.png"
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
