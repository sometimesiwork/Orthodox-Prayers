# Screenshots at several window sizes. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class R {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
}
"@

Get-Process OrthodoxPrayers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
Remove-Item "$env:APPDATA\OrthodoxPrayers\settings.ini" -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 2300
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[R]::ShowWindow($hwnd, 9) | Out-Null   # SW_RESTORE
[R]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 600

function Shot([string]$file) {
    $r = New-Object R+RECT
    [R]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path (Resolve-Path "build").Path $file), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "  saved build\$file"
}

# минимальный размер окна
[R]::SetWindowPos($hwnd, [IntPtr]::Zero, 100, 60, 1040, 620, 0x0040) | Out-Null
Start-Sleep -Milliseconds 700
Shot "ui_18_narrow.png"

# обычный размер
[R]::SetWindowPos($hwnd, [IntPtr]::Zero, 100, 40, 1360, 860, 0x0040) | Out-Null
Start-Sleep -Milliseconds 700
Shot "ui_19_normal.png"

# развёрнутое окно
[R]::ShowWindow($hwnd, 3) | Out-Null
Start-Sleep -Milliseconds 800
Shot "ui_20_maximized.png"

$proc.CloseMainWindow() | Out-Null
Start-Sleep -Seconds 1
