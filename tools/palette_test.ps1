# Palette visual check: "Monastery stone" (2) and "Azure" (3). ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class PT {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
$ini = Join-Path $env:APPDATA "OrthodoxPrayers\settings.ini"

function Set-Theme([int]$idx, [string]$section) {
    $t = [System.IO.File]::ReadAllText($ini)
    $t = $t -replace 'theme=[^\r\n]*', "theme=$idx"
    $t = $t -replace 'section=[^\r\n]*', "section=$section"
    [System.IO.File]::WriteAllText($ini, $t, (New-Object System.Text.UTF8Encoding($false)))
}

function Shot([string]$file) {
    $r = New-Object PT+RECT
    [PT]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $g.Dispose()
    $bmp.Save((Join-Path $PWD "build\$file"), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "saved $file"
}

function ClickWin([int]$cx, [int]$cy) {
    $r = New-Object PT+RECT
    [PT]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    [PT]::SetCursorPos(($r.Left + $cx), ($r.Top + $cy)) | Out-Null
    Start-Sleep -Milliseconds 150
    [PT]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 70
    [PT]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 450
}

function Run-Palette([int]$idx, [string]$tag, [string]$section) {
    Get-Process OrthodoxPrayers -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 600
    Set-Theme $idx $section
    Start-Process -FilePath $Exe | Out-Null
    Start-Sleep -Milliseconds 2800
    $script:proc = Get-Process OrthodoxPrayers | Select-Object -First 1
    $script:hwnd = $proc.MainWindowHandle
    [PT]::SetForegroundWindow($hwnd) | Out-Null
    Start-Sleep -Milliseconds 700
    [PT]::SetCursorPos(300, 500) | Out-Null
    Start-Sleep -Milliseconds 300
    Shot "$tag.png"
    Stop-Process -Id $proc.Id -Force
    Start-Sleep -Milliseconds 500
}

Run-Palette 2 "p2-stone-cal" "kalendar"
Run-Palette 2 "p2-stone-chetki" "chetki"
Run-Palette 3 "p3-azure-cal" "kalendar"
Run-Palette 3 "p3-azure-chetki" "chetki"
Set-Theme 1 "kalendar"
Write-Host done
