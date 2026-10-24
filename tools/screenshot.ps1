# Screenshot of the app window via PrintWindow (foreground-independent).
# ASCII-only on purpose.
param(
    [string]$Exe = ".\build\OrthodoxPrayers.exe",
    [string]$Out = ".\build\screenshot.png",
    [int]$DelayMs = 2600
)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class WinP {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds $DelayMs
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
if ($hwnd -eq [IntPtr]::Zero) {
    Write-Host "No window handle (exited: $($proc.HasExited))"
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    exit 1
}
$r = New-Object WinP+RECT
[WinP]::GetWindowRect($hwnd, [ref]$r) | Out-Null
$w = $r.Right - $r.Left
$h = $r.Bottom - $r.Top
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
[WinP]::PrintWindow($hwnd, $dc, 2) | Out-Null
$g.ReleaseHdc($dc)
$dir = (Resolve-Path (Split-Path $Out -Parent)).Path
$bmp.Save($dir + "\" + (Split-Path $Out -Leaf), [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host "saved: $Out ($w x $h)"
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
