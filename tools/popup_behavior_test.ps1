# Checks: Esc closes popup, popup follows window move, toggle by pill click.
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DP11 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowExW(IntPtr p, IntPtr a, string cls, string t);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
function Click([int]$x, [int]$y) {
    [DP11]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 140
    [DP11]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [DP11]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
}
function Find-Popup {
    for ($i = 0; $i -lt 3; $i++) {
        $p = [DP11]::FindWindowExW([IntPtr]::Zero, [IntPtr]::Zero, "OPDatePopup", $null)
        if ($p -ne [IntPtr]::Zero) { return $p }
        Start-Sleep -Milliseconds 400
    }
    return [IntPtr]::Zero
}
$proc = Start-Process -FilePath ".\build\OrthodoxPrayers.exe" -PassThru
Start-Sleep -Milliseconds 3200
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[DP11]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null
Start-Sleep -Milliseconds 250
[DP11]::SetWindowPos($hwnd, [IntPtr](-2), 0, 0, 0, 0, 0x0001 -bor 0x0002) | Out-Null
Start-Sleep -Milliseconds 300
$cr = New-Object DP11+RECT
[DP11]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$pill = New-Object DP11+POINT; $pill.X = $cr.Right - 113; $pill.Y = 66
[DP11]::ClientToScreen($hwnd, [ref]$pill) | Out-Null
$tab = New-Object DP11+POINT; $tab.X = 330; $tab.Y = 24
[DP11]::ClientToScreen($hwnd, [ref]$tab) | Out-Null
Click $tab.X $tab.Y
Start-Sleep -Milliseconds 800

# 1) toggle open/close/open
Click $pill.X $pill.Y
$p = Find-Popup
Write-Host ("toggle1 open: {0}" -f ($(if ($p -ne [IntPtr]::Zero) { "yes" } else { "NO" })))
Click $pill.X $pill.Y
Start-Sleep -Milliseconds 600
$p = Find-Popup
Write-Host ("toggle2 close: {0}" -f ($(if ($p -eq [IntPtr]::Zero) { "yes (closed)" } else { "NO (still open)" })))

# 2) Esc closes
Click $pill.X $pill.Y
$p = Find-Popup
if ($p -ne [IntPtr]::Zero) {
    [DP11]::PostMessageW($hwnd, 0x0100, [IntPtr]0x1B, [IntPtr]0) | Out-Null
    [DP11]::PostMessageW($hwnd, 0x0101, [IntPtr]0x1B, [IntPtr]0) | Out-Null
    Start-Sleep -Milliseconds 600
    $p = Find-Popup
    Write-Host ("esc: {0}" -f ($(if ($p -eq [IntPtr]::Zero) { "closed (good)" } else { "STILL OPEN" })))
}

# 3) popup follows window move
Click $pill.X $pill.Y
$p = Find-Popup
if ($p -ne [IntPtr]::Zero) {
    $pr1 = New-Object DP11+RECT
    [DP11]::GetWindowRect($p, [ref]$pr1) | Out-Null
    $wr = New-Object DP11+RECT
    [DP11]::GetWindowRect($hwnd, [ref]$wr) | Out-Null
    [DP11]::SetWindowPos($hwnd, [IntPtr]::Zero, $wr.Left - 80, $wr.Top + 30, 0, 0, 0x0001 -bor 0x0004 -bor 0x0010) | Out-Null
    Start-Sleep -Milliseconds 600
    $p2 = Find-Popup
    if ($p2 -ne [IntPtr]::Zero) {
        $pr2 = New-Object DP11+RECT
        [DP11]::GetWindowRect($p2, [ref]$pr2) | Out-Null
        $dx = $pr2.Left - $pr1.Left
        $dy = $pr2.Top - $pr1.Top
        Write-Host ("move: popup dx={0} dy={1} (expected dx=-80 dy=30)" -f $dx, $dy)
    } else {
        Write-Host "move: popup disappeared (bug)"
    }
}
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
