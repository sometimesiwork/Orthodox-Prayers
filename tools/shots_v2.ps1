# Capture screenshots of the main sections (real clicks). ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class SH {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
function Shot([IntPtr]$hwnd, [string]$file) {
    $r = New-Object SH+RECT
    [SH]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $dc = $g.GetHdc()
    [SH]::PrintWindow($hwnd, $dc, 2) | Out-Null
    $g.ReleaseHdc($dc)
    $dir = (Resolve-Path (Split-Path $file -Parent)).Path
    $bmp.Save((Join-Path $dir (Split-Path $file -Leaf)), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "saved $file"
}
function ClickClient([IntPtr]$hwnd, [int]$x, [int]$y) {
    $p = New-Object SH+POINT; $p.X = $x; $p.Y = $y
    [SH]::ClientToScreen($hwnd, [ref]$p) | Out-Null
    [SH]::SetCursorPos($p.X, $p.Y) | Out-Null
    Start-Sleep -Milliseconds 120
    [SH]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 60
    [SH]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 450
}
Start-Process -FilePath $Exe | Out-Null
Start-Sleep -Milliseconds 2600
$proc = Get-Process OrthodoxPrayers | Select-Object -First 1
$hwnd = $proc.MainWindowHandle
[SH]::ShowWindow($hwnd, 3) | Out-Null
[SH]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 900
$cr = New-Object SH+RECT
[SH]::GetClientRect($hwnd, [ref]$cr) | Out-Null
Write-Host "client: $($cr.Right) x $($cr.Bottom)"

Shot $hwnd ".\build\v2_max_calendar.png"
ClickClient $hwnd 343 25   # quick tabs row
Shot $hwnd ".\build\v2_calendar.png"
ClickClient $hwnd 437 25
Shot $hwnd ".\build\v2_sluzhby.png"
ClickClient $hwnd 517 25
Start-Sleep -Milliseconds 1800
Shot $hwnd ".\build\v2_azbyka.png"
ClickClient $hwnd 594 25
Shot $hwnd ".\build\v2_bible.png"
ClickClient $hwnd 661 25
Shot $hwnd ".\build\v2_pominalnik.png"
# dark theme
ClickClient $hwnd ($cr.Right - 31) 24
Shot $hwnd ".\build\v2_pominalnik_dark.png"
ClickClient $hwnd ($cr.Right - 31) 24
# collapsed sidebar
ClickClient $hwnd 244 38
Shot $hwnd ".\build\v2_collapsed.png"
ClickClient $hwnd 28 38
Stop-Process -Id $proc.Id -Force
Write-Host done
