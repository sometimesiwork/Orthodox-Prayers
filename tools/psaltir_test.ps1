# Checks the Psalter with translation and other cleaned sections. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class P {
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
[P]::ShowWindow($hwnd, 3) | Out-Null
[P]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 700

$cr = New-Object P+RECT
[P]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$cw = $cr.Right

function Shot([string]$file) {
    $r = New-Object P+RECT
    [P]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path (Resolve-Path "build").Path $file), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "  saved build\$file"
}

function Click([int]$x, [int]$y) {
    $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
    [P]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp) | Out-Null
    [P]::PostMessage($hwnd, 0x0202, [IntPtr]0, $lp) | Out-Null
    Start-Sleep -Milliseconds 2000
}

# Псалтирь (списка разделов: Псалтирь — третий пункт)
Click 120 233
Shot "ui_28_psaltir_ru.png"

# переключаем на церковнославянский
Click ($cw - 261) 24
Shot "ui_29_psaltir_cs.png"

Stop-Process -Id $proc.Id -Force
Write-Host "done"
