# Stress test: clicks every control and checks the app survives. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Windows.Forms
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
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
}
"@

Get-Process OrthodoxPrayers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 2300
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[T]::ShowWindow($hwnd, 3) | Out-Null
[T]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 700

$cr = New-Object T+RECT
[T]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$cw = $cr.Right

function Click([int]$x, [int]$y) {
    $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
    [T]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp) | Out-Null
    [T]::PostMessage($hwnd, 0x0202, [IntPtr]0, $lp) | Out-Null
    Start-Sleep -Milliseconds 120
}

# все кнопки шапки
foreach ($dx in @(38, 82, 126, 170, 214)) { Click ($cw - $dx) 39 }
# открыть/закрыть панели
Click ($cw - 126) 39
Click 900 300
Click ($cw - 82) 39
Click 900 300
# клики по боковой панели
foreach ($y in @(150, 200, 250, 300, 350, 400, 450, 500)) { Click 120 $y }
# клики по содержимому, включая звёздочки
foreach ($y in @(190, 260, 340, 420, 500, 580)) {
    Click 1000 $y
    Click 1490 $y
    Click 1490 $y
}
# колесо мыши
for ($i = 0; $i -lt 12; $i++) {
    $lp = [IntPtr]([int](-120) -shl 16)
    [T]::PostMessage($hwnd, 0x020A, $lp, [IntPtr]1000) | Out-Null
    Start-Sleep -Milliseconds 60
}
# правый клик (контекстное меню)
$lp = [IntPtr]((300 -shl 16) -bor (1000 -band 0xFFFF))
[T]::PostMessage($hwnd, 0x0205, [IntPtr]0, $lp) | Out-Null
Start-Sleep -Milliseconds 500
# закрыть меню клавишей Esc
[System.Windows.Forms.SendKeys]::SendWait("{ESC}") 2>$null
Start-Sleep -Milliseconds 300

Start-Sleep -Milliseconds 700
$alive = -not $proc.HasExited
Write-Host ("app alive: " + $alive)
if (-not $alive) { Write-Host ("exit code: " + $proc.ExitCode) }
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
