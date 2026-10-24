# Regression: pick a date, then navigate to another month and pick again.
# The bug: offset was added to the current offset (double counting).
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DP10 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowExW(IntPtr p, IntPtr a, string cls, string t);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int w, int hh, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
function Click([int]$x, [int]$y) {
    [DP10]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 140
    [DP10]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [DP10]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
}
function Find-Popup {
    for ($i = 0; $i -lt 3; $i++) {
        $p = [DP10]::FindWindowExW([IntPtr]::Zero, [IntPtr]::Zero, "OPDatePopup", $null)
        if ($p -ne [IntPtr]::Zero) { return $p }
        Start-Sleep -Milliseconds 500
    }
    return [IntPtr]::Zero
}
function Shot([IntPtr]$hwnd, [string]$file) {
    $r = New-Object DP10+RECT
    [DP10]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $dc = $g.GetHdc()
    [DP10]::PrintWindow($hwnd, $dc, 2) | Out-Null
    $g.ReleaseHdc($dc)
    $bmp.Save((Join-Path (Resolve-Path (Split-Path $file -Parent)).Path (Split-Path $file -Leaf)), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "saved: $file"
}
$proc = Start-Process -FilePath ".\build\OrthodoxPrayers.exe" -PassThru
Start-Sleep -Milliseconds 3200
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
if ($hwnd -eq [IntPtr]::Zero) { Write-Host "no window"; Stop-Process -Id $proc.Id -Force; exit 1 }
[DP10]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null
Start-Sleep -Milliseconds 250
[DP10]::SetWindowPos($hwnd, [IntPtr](-2), 0, 0, 0, 0, 0x0001 -bor 0x0002) | Out-Null
Start-Sleep -Milliseconds 300
$cr = New-Object DP10+RECT
[DP10]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$pill = New-Object DP10+POINT
$pill.X = $cr.Right - 113; $pill.Y = 66
[DP10]::ClientToScreen($hwnd, [ref]$pill) | Out-Null

# make sure Calendar section is active
$tab = New-Object DP10+POINT; $tab.X = 330; $tab.Y = 24
[DP10]::ClientToScreen($hwnd, [ref]$tab) | Out-Null
Click $tab.X $tab.Y
Start-Sleep -Milliseconds 800

function Open-Popup {
    Click $pill.X $pill.Y
    $p = Find-Popup
    if ($p -eq [IntPtr]::Zero) { Write-Host "popup NOT open"; return $null }
    $r = New-Object DP10+RECT
    [DP10]::GetWindowRect($p, [ref]$r) | Out-Null
    return $r
}
function Cell-Rect([object]$r, [int]$row, [int]$col) {
    return @(($r.Left + 14 + $col * 33 + 16), ($r.Top + 56 + $row * 30 + 15))
}

# 1) pick 14 October (row 2, col 2)
$r = Open-Popup
if ($null -eq $r) { Stop-Process -Id $proc.Id -Force; exit 1 }
$c = Cell-Rect $r 2 2
Click $c[0] $c[1]
Start-Sleep -Milliseconds 900
Write-Host "step1: picked 14 Oct"

# 2) open popup, go to November, pick Nov 15 (row 2, col 6)
$r = Open-Popup
if ($null -eq $r) { Stop-Process -Id $proc.Id -Force; exit 1 }
$navX = $r.Left + 259 - 14 - 30 + 15; $navY = $r.Top + 17
Click $navX $navY      # Oct -> Nov
Start-Sleep -Milliseconds 500
$c = Cell-Rect $r 2 6  # Nov 15 (Sunday)
Click $c[0] $c[1]
Start-Sleep -Milliseconds 900
Shot $hwnd ".\build\ui_pick_nov.png"
Write-Host "step2: picked 15 Nov (expected pill: 15 ноя / 2 ноя ст.)"

# 3) open (November), go two months -> January 2027, pick Jan 5 (row 1, col 1)
$r = Open-Popup
if ($null -eq $r) { Stop-Process -Id $proc.Id -Force; exit 1 }
Click $navX $navY
Start-Sleep -Milliseconds 400
Click $navX $navY
Start-Sleep -Milliseconds 400
$c = Cell-Rect $r 1 1  # Jan 5, 2027 (Tuesday)
Click $c[0] $c[1]
Start-Sleep -Milliseconds 900
Shot $hwnd ".\build\ui_pick_jan.png"
Write-Host "step3: picked 5 Jan 2027 (expected pill: 5 янв / 23 дек ст.)"

# 4) Esc closes popup
$r = Open-Popup
if ($null -ne $r) {
    [DP10]::PostMessageW($hwnd, 0x0100, [IntPtr]0x1B, [IntPtr]0) | Out-Null
    [DP10]::PostMessageW($hwnd, 0x0101, [IntPtr]0x1B, [IntPtr]0) | Out-Null
    Start-Sleep -Milliseconds 600
    $p = [DP10]::FindWindowExW([IntPtr]::Zero, [IntPtr]::Zero, "OPDatePopup", $null)
    Write-Host ($(if ($p -eq [IntPtr]::Zero) { "Esc: popup closed (good)" } else { "Esc: popup OPEN (bug)" }))
}
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
