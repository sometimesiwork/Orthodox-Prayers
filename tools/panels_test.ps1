# Проверка новых панелей и контекстного меню. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class N {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 2300
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[N]::ShowWindow($hwnd, 3) | Out-Null
[N]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 700

$cr = New-Object N+RECT
[N]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$cw = $cr.Right

function Shot([string]$file) {
    $r = New-Object N+RECT
    [N]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path (Resolve-Path "build").Path $file), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "  saved build\$file"
}

function Click([int]$x, [int]$y) {
    $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
    [N]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp) | Out-Null
    [N]::PostMessage($hwnd, 0x0202, [IntPtr]0, $lp) | Out-Null
    Start-Sleep -Milliseconds 450
}

# содержание
Click ($cw - 126) 24
Shot "ui_10_contents.png"

# настройки
Click ($cw - 126) 24
Click ($cw - 82) 24
Shot "ui_11_settings.png"

# закрыть панель и открыть контекстное меню карточки
Click ($cw - 82) 24
$lp = [IntPtr]((200 -shl 16) -bor (1000 -band 0xFFFF))
[N]::PostMessage($hwnd, 0x0205, [IntPtr]0, $lp) | Out-Null
Start-Sleep -Milliseconds 700
Shot "ui_12_context.png"

Stop-Process -Id $proc.Id -Force
Write-Host "done"
