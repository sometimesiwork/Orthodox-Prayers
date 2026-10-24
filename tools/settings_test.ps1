# Verifies that the settings panel chips and the contents list are clickable. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class S {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
}
"@

Get-Process OrthodoxPrayers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
Remove-Item "$env:APPDATA\OrthodoxPrayers\settings.ini" -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 2300
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[S]::ShowWindow($hwnd, 3) | Out-Null
[S]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 700

$cr = New-Object S+RECT
[S]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$cw = $cr.Right

function Click([int]$x, [int]$y) {
    $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
    [S]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp) | Out-Null
    [S]::PostMessage($hwnd, 0x0202, [IntPtr]0, $lp) | Out-Null
    Start-Sleep -Milliseconds 350
}

function Shot([string]$file) {
    $r = New-Object S+RECT
    [S]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path (Resolve-Path "build").Path $file), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}

# --- панель настроек ---
Click ($cw - 82) 24
Shot "ui_11_settings.png"

Click ($cw - 200) 140   # шрифт Georgia
Click ($cw - 75) 238    # межстрочный интервал «Свободно»
Click ($cw - 75) 302    # ширина колонки «Широкая»
Click ($cw - 100) 366   # оформление «Светлая»
Shot "ui_16_settings_applied.png"

# --- содержание: переход к молитве ---
Click ($cw - 126) 24    # кнопка «Содержание»
Shot "ui_10_contents.png"
Click ($cw - 200) 300   # строка списка (молитва)
Shot "ui_17_contents_jump.png"

$proc.CloseMainWindow() | Out-Null
Start-Sleep -Seconds 2

Write-Host "--- settings.ini ---"
Get-Content "$env:APPDATA\OrthodoxPrayers\settings.ini" -ErrorAction SilentlyContinue
