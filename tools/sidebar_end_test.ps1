# Checks that the last menu item is fully visible after scrolling to the end. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class L {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
}
"@

Get-Process OrthodoxPrayers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
Remove-Item "$env:APPDATA\OrthodoxPrayers\settings.ini" -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 2300
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[L]::ShowWindow($hwnd, 3) | Out-Null
[L]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 800

$wr = New-Object L+RECT
[L]::GetWindowRect($hwnd, [ref]$wr) | Out-Null

function Shot([string]$file) {
    $r = New-Object L+RECT
    [L]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap 280, ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path (Resolve-Path "build").Path $file), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}

# долго крутим колесо над меню
for ($i = 0; $i -lt 30; $i++) {
    $wp = [IntPtr]((-120 -shl 16) -band 0xFFFFFFFF)
    $lp = [IntPtr]((($wr.Top + 500) -shl 16) -bor (($wr.Left + 120) -band 0xFFFF))
    [L]::PostMessage($hwnd, 0x020A, $wp, $lp) | Out-Null
    Start-Sleep -Milliseconds 60
}
Start-Sleep -Milliseconds 600
Shot "ui_48_sidebar_end.png"
Write-Host "saved build\ui_48_sidebar_end.png"
$proc.CloseMainWindow() | Out-Null
Start-Sleep -Seconds 1
Write-Host "done"
