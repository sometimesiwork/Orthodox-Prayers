# Final UI check: favourites, scroll, zoom. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class F {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

function Shot([IntPtr]$hwnd, [string]$file) {
    $r = New-Object F+RECT
    [F]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path (Resolve-Path "build").Path $file), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "  saved build\$file"
}

function Click([IntPtr]$hwnd, [int]$x, [int]$y) {
    $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
    [F]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp) | Out-Null
    [F]::PostMessage($hwnd, 0x0202, [IntPtr]0, $lp) | Out-Null
    Start-Sleep -Milliseconds 450
}

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 2300
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[F]::ShowWindow($hwnd, 3) | Out-Null
[F]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 700

$cr = New-Object F+RECT
[F]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$cw = $cr.Right

# звезда у первой молитвы
Click $hwnd 1490 160
Shot $hwnd "ui_6_favorite.png"

# увеличение шрифта
Click $hwnd ($cw - 82) 24
Click $hwnd ($cw - 82) 24
Shot $hwnd "ui_7_bigfont.png"

# раздел «Основные молитвы»
Click $hwnd 120 393
Shot $hwnd "ui_8_basics.png"

Stop-Process -Id $proc.Id -Force
Write-Host "done"
