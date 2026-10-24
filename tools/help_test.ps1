# Проверка справки (F1) и финальных скриншотов. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class H {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  public const uint KEYUP = 2;
}
"@

Get-Process OrthodoxPrayers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 2300
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[H]::ShowWindow($hwnd, 3) | Out-Null
[H]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 800

function Shot([string]$file) {
    $r = New-Object H+RECT
    [H]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path (Resolve-Path "build").Path $file), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "  saved build\$file"
}

# F1 через клавиатуру
[H]::keybd_event(0x70, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 80
[H]::keybd_event(0x70, 0, [H]::KEYUP, [IntPtr]::Zero)
Start-Sleep -Milliseconds 700
Shot "ui_14_help.png"

# закрыть справку кликом в пустое место
$cr = New-Object H+RECT
[H]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$lp = [IntPtr]((700 -shl 16) -bor (400 -band 0xFFFF))
[H]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp) | Out-Null
[H]::PostMessage($hwnd, 0x0202, [IntPtr]0, $lp) | Out-Null
Start-Sleep -Milliseconds 600
Shot "ui_15_main.png"

Stop-Process -Id $proc.Id -Force
Write-Host "done"
