# Capture screenshots of sections/panels via CopyFromScreen (ASCII-only).
param([string]$Exe = ".\build\OrthodoxPrayers.exe")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class SH3 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
function Shot([IntPtr]$hwnd, [string]$file) {
    $r = New-Object SH3+RECT
    [SH3]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $g.Dispose()
    $dir = (Resolve-Path (Split-Path $file -Parent)).Path
    $bmp.Save((Join-Path $dir (Split-Path $file -Leaf)), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "saved $file"
}
function ClickClient([IntPtr]$hwnd, [int]$x, [int]$y) {
    $p = New-Object SH3+POINT; $p.X = $x; $p.Y = $y
    [SH3]::ClientToScreen($hwnd, [ref]$p) | Out-Null
    [SH3]::SetCursorPos($p.X, $p.Y) | Out-Null
    Start-Sleep -Milliseconds 120
    [SH3]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 60
    [SH3]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 450
}
Start-Process -FilePath $Exe | Out-Null
Start-Sleep -Milliseconds 2600
$proc = Get-Process OrthodoxPrayers | Select-Object -First 1
$hwnd = $proc.MainWindowHandle
[SH3]::ShowWindow($hwnd, 3) | Out-Null
[SH3]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 900
$cr = New-Object SH3+RECT
[SH3]::GetClientRect($hwnd, [ref]$cr) | Out-Null

Shot $hwnd ".\build\v3_calendar.png"
ClickClient $hwnd 437 25
Shot $hwnd ".\build\v3_sluzhby.png"
ClickClient $hwnd 517 25
Start-Sleep -Milliseconds 1600
Shot $hwnd ".\build\v3_azbyka.png"
ClickClient $hwnd 594 25
Shot $hwnd ".\build\v3_bible.png"
ClickClient $hwnd 661 25
Shot $hwnd ".\build\v3_pominalnik.png"
# chetki via sidebar bottom: scroll sidebar to bottom then click last item
ClickClient $hwnd 130 ($cr.Bottom - 250)
[System.Windows.Forms.SendKeys]::SendWait("{END}") 2>$null
# scroll sidebar with wheel
[SH3]::SetCursorPos(($(New-Object SH3+POINT).X), 0) | Out-Null
Start-Sleep -Milliseconds 200
# switch to dark theme
ClickClient $hwnd ($cr.Right - 31) 24
Shot $hwnd ".\build\v3_calendar_dark.png"
ClickClient $hwnd ($cr.Right - 31) 24
Stop-Process -Id $proc.Id -Force
Write-Host done
