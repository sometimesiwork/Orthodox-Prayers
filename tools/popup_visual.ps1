# Visual check of the custom popup: open, screenshot, click next/prev, screenshot again.
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DP8 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowExW(IntPtr p, IntPtr a, string cls, string t);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
function Click([int]$x, [int]$y) {
    [DP8]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 140
    [DP8]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [DP8]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
}
function Shot-Region([int]$x, [int]$y, [int]$w, [int]$h, [string]$file) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($x, $y, 0, 0, $bmp.Size) | Out-Null
    $bmp.Save((Join-Path (Resolve-Path (Split-Path $file -Parent)).Path (Split-Path $file -Leaf)), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "saved: $file"
}
function Find-Popup {
    for ($i = 0; $i -lt 3; $i++) {
        $p = [DP8]::FindWindowExW([IntPtr]::Zero, [IntPtr]::Zero, "OPDatePopup", $null)
        if ($p -ne [IntPtr]::Zero) { return $p }
        Start-Sleep -Milliseconds 500
    }
    return [IntPtr]::Zero
}
$proc = Start-Process -FilePath ".\build\OrthodoxPrayers.exe" -PassThru
Start-Sleep -Milliseconds 3200
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
if ($hwnd -eq [IntPtr]::Zero) { Write-Host "no window"; Stop-Process -Id $proc.Id -Force; exit 1 }
$cr = New-Object DP8+RECT
[DP8]::GetClientRect($hwnd, [ref]$cr) | Out-Null

# ensure Kalendar section is active
$tab = New-Object DP8+POINT
$tab.X = 330; $tab.Y = 54
[DP8]::ClientToScreen($hwnd, [ref]$tab) | Out-Null
Click $tab.X $tab.Y
Start-Sleep -Milliseconds 900

# open popup
$pt = New-Object DP8+POINT
$pt.X = $cr.Right - 113; $pt.Y = 66
[DP8]::ClientToScreen($hwnd, [ref]$pt) | Out-Null
Click $pt.X $pt.Y
$popup = Find-Popup
if ($popup -eq [IntPtr]::Zero) { Write-Host "popup NOT open"; Stop-Process -Id $proc.Id -Force; exit 1 }
$r = New-Object DP8+RECT
[DP8]::GetWindowRect($popup, [ref]$r) | Out-Null
Write-Host ("popup: {0},{1} {2}x{3}" -f $r.Left, $r.Top, ($r.Right-$r.Left), ($r.Bottom-$r.Top))
Shot-Region ($r.Left - 6) ($r.Top - 6) ($r.Right - $r.Left + 12) ($r.Bottom - $r.Top + 12) ".\build\popup_view_oct.png"

# hover a day cell to check hover highlight
$hx = $r.Left + 14 + 3 * 33 + 16
$hy = $r.Top + 56 + 1 * 30 + 15
[DP8]::SetCursorPos($hx, $hy) | Out-Null
Start-Sleep -Milliseconds 400
Shot-Region ($r.Left - 6) ($r.Top - 6) ($r.Right - $r.Left + 12) ($r.Bottom - $r.Top + 12) ".\build\popup_hover.png"

# next month
$navX = $r.Left + 259 - 14 - 30 + 15
$navY = $r.Top + 17
Click $navX $navY
Start-Sleep -Milliseconds 500
Shot-Region ($r.Left - 6) ($r.Top - 6) ($r.Right - $r.Left + 12) ($r.Bottom - $r.Top + 12) ".\build\popup_view_nov.png"
if ([DP8]::FindWindowExW([IntPtr]::Zero, [IntPtr]::Zero, "OPDatePopup", $null) -ne [IntPtr]::Zero) {
    Write-Host "nav-next: popup still open (good)"
} else {
    Write-Host "nav-next: popup CLOSED (bug)"
}
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
