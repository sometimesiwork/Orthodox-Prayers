# Screenshots of the newly added sections. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class N {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
}
"@

Get-Process OrthodoxPrayers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
Remove-Item "$env:APPDATA\OrthodoxPrayers\settings.ini" -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 2500
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[N]::ShowWindow($hwnd, 3) | Out-Null
[N]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 800

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
    Start-Sleep -Milliseconds 1300
}

# список разделов: утро 112, вечер 158, псалтирь 204, чин12 250, тропари 296,
# акафист 342, канон 388, ангел 434, богородице 480, пасха 526, причастие 572, богорад 618
Click 120 526   # Пасхальный канон
Shot "ui_31_pasxa.png"
Click 120 572   # К Святому Причащению
Shot "ui_32_prichastie.png"
Click 120 434   # Канон Ангелу-Хранителю
Shot "ui_33_kanon_angel.png"
Click 120 480   # Канон молебный ко Богородице
Shot "ui_34_kanon_bogorodice.png"

$proc.CloseMainWindow() | Out-Null
Start-Sleep -Seconds 1
