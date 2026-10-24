# Rosary ring + toast + settings panel visual test. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class RT {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int w, int hh, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
function Shot([string]$file) {
    $r = New-Object RT+RECT
    [RT]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $g.Dispose()
    $bmp.Save((Join-Path $PWD "build\$file"), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "saved $file"
}
function ClickCapture([int]$cx, [int]$cy) {
    $o = New-Object RT+POINT; $o.X = 0; $o.Y = 0
    [RT]::ClientToScreen($hwnd, [ref]$o) | Out-Null
    $r = New-Object RT+RECT
    [RT]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $x = $r.Left + $cx
    $y = $r.Top + $cy
    [RT]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 140
    [RT]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 70
    [RT]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 400
}
Start-Process -FilePath $Exe | Out-Null
Start-Sleep -Milliseconds 2800
$proc = Get-Process OrthodoxPrayers | Select-Object -First 1
$hwnd = $proc.MainWindowHandle
[RT]::SetForegroundWindow($hwnd) | Out-Null
[RT]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null
Start-Sleep -Milliseconds 250
[RT]::SetWindowPos($hwnd, [IntPtr](-2), 0, 0, 0, 0, 0x0001 -bor 0x0002) | Out-Null
Start-Sleep -Milliseconds 800

# switch to the Chetki section (quick tab)
ClickCapture 694 54
Start-Sleep -Milliseconds 500

# ring center (window capture coordinates)
ClickCapture 408 350
Shot "v12_ring13.png"

# previous-day arrow -> toast
ClickCapture 994 96
Start-Sleep -Milliseconds 220
Shot "v12_toast_in.png"
Start-Sleep -Milliseconds 3200
Shot "v12_toast_gone.png"

# settings panel (client coords)
$cr = New-Object RT+RECT
[RT]::GetClientRect($hwnd, [ref]$cr) | Out-Null
ClickCapture ($cr.Right - 82 + 8) (24 + 31)
Start-Sleep -Milliseconds 400
Shot "v12_settings.png"

Stop-Process -Id $proc.Id -Force
Write-Host done
