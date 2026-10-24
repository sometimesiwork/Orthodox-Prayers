# Calendar grid interaction test: clicks a day, month arrows, Today. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class CG {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int w, int hh, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
function Shot([string]$file) {
    $r = New-Object CG+RECT
    [CG]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $g.Dispose()
    $bmp.Save((Join-Path $PWD "build\$file"), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "saved $file"
}
function ClickAt([int]$x, [int]$y) {
    [CG]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 150
    [CG]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 70
    [CG]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 700
}
Start-Process -FilePath $Exe | Out-Null
Start-Sleep -Milliseconds 2800
$proc = Get-Process OrthodoxPrayers | Select-Object -First 1
$hwnd = $proc.MainWindowHandle
[CG]::SetForegroundWindow($hwnd) | Out-Null
[CG]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null
Start-Sleep -Milliseconds 250
[CG]::SetWindowPos($hwnd, [IntPtr](-2), 0, 0, 0, 0, 0x0001 -bor 0x0002) | Out-Null
Start-Sleep -Milliseconds 800
$r = New-Object CG+RECT
[CG]::GetWindowRect($hwnd, [ref]$r) | Out-Null
Write-Host "window: $($r.Left),$($r.Top) $($r.Right - $r.Left)x$($r.Bottom - $r.Top)"

# switch to the Calendar section (quick tab, capture-relative coords)
ClickAt ($r.Left + 340) ($r.Top + 54)

# capture-relative coordinates measured from v8_cal_1240.png
# cell 15 (row 2, col 3): capture (742, 341)
ClickAt ($r.Left + 742) ($r.Top + 341)
Shot "v9_after_day15.png"

# next month arrow: capture (1153, 200)
ClickAt ($r.Left + 1153) ($r.Top + 200)
Shot "v9_after_next.png"

# previous month twice: capture (1111, 200)
ClickAt ($r.Left + 1111) ($r.Top + 200)
ClickAt ($r.Left + 1111) ($r.Top + 200)
Shot "v9_after_prev2.png"

# Today chip: capture (1036, 200)
ClickAt ($r.Left + 1036) ($r.Top + 200)
Shot "v9_after_today.png"

Stop-Process -Id $proc.Id -Force
Write-Host done
