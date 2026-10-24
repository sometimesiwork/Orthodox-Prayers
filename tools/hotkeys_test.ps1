# Hotkeys test: real Ctrl+1..9 presses (ASCII-only, no BOM).
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class C {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  public const uint KEYUP = 2;
}
"@

Get-Process OrthodoxPrayers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
Remove-Item "$env:APPDATA\OrthodoxPrayers\settings.ini" -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 2500
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[C]::ShowWindow($hwnd, 3) | Out-Null
[C]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 800

function Shot([string]$file) {
    $r = New-Object C+RECT
    [C]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path (Resolve-Path "build").Path $file), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "  saved build\$file"
}

# клик по содержимому — чтобы фокус был у главного окна
$lp = [IntPtr]((400 -shl 16) -bor (900 -band 0xFFFF))
[C]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp) | Out-Null
[C]::PostMessage($hwnd, 0x0202, [IntPtr]0, $lp) | Out-Null
Start-Sleep -Milliseconds 600

# Ctrl + 3
[C]::keybd_event(0x11, 0, 0, [IntPtr]::Zero)       # Ctrl down
Start-Sleep -Milliseconds 100
[C]::keybd_event(0x33, 0, 0, [IntPtr]::Zero)       # '3' down
Start-Sleep -Milliseconds 120
[C]::keybd_event(0x33, 0, [C]::KEYUP, [IntPtr]::Zero)
[C]::keybd_event(0x11, 0, [C]::KEYUP, [IntPtr]::Zero)
Start-Sleep -Milliseconds 1200
Shot "ui_40_ctrl3.png"

$proc.CloseMainWindow() | Out-Null
Start-Sleep -Seconds 2
Get-Content "$env:APPDATA\OrthodoxPrayers\settings.ini" -ErrorAction SilentlyContinue | Select-String "section"
