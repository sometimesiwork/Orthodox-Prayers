# Скриншоты раздела «Тропари праздников» (ЦС и перевод). ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class T {
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
Start-Sleep -Milliseconds 2500
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[T]::ShowWindow($hwnd, 3) | Out-Null
[T]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 800

function Shot([string]$file) {
    $r = New-Object T+RECT
    [T]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path (Resolve-Path "build").Path $file), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "  saved build\$file"
}

function Click([int]$x, [int]$y) {
    $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
    [T]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp) | Out-Null
    [T]::PostMessage($hwnd, 0x0202, [IntPtr]0, $lp) | Out-Null
    Start-Sleep -Milliseconds 900
}

$cr = New-Object T+RECT
[T]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$cw = $cr.Right

# раздел «Тропари праздников» в группе «СЛУЖБЫ»
Click 130 486
Shot "ui_41_troparia_ru.png"

# церковнославянский текст (кнопка «ЦС» в шапке)
Click ($cw - 261) 24
Shot "ui_42_troparia_cs.png"

$proc.CloseMainWindow() | Out-Null
Start-Sleep -Seconds 1
Get-Content "$env:APPDATA\OrthodoxPrayers\settings.ini" -ErrorAction SilentlyContinue | Select-String "section"
