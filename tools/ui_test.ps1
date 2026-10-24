# UI smoke test: clicks buttons, types in search, captures screenshots. ASCII-only.
param([string]$Exe = ".\build\OrthodoxPrayers.exe")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class U {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll", EntryPoint="PostMessageW")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool SetWindowText(IntPtr h, string t);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder t, int m);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int w, int hh, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

function Shot([IntPtr]$hwnd, [string]$file) {
    $r = New-Object U+RECT
    [U]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $dir = Split-Path -Parent $file
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
    $bmp.Save((Join-Path (Resolve-Path $dir).Path (Split-Path $file -Leaf)), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "  saved $file"
}

function Click([IntPtr]$hwnd, [int]$x, [int]$y) {
    $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
    [U]::PostMessage($hwnd, 0x0201, [IntPtr]1, $lp) | Out-Null
    [U]::PostMessage($hwnd, 0x0202, [IntPtr]0, $lp) | Out-Null
    Start-Sleep -Milliseconds 400
}

function TypeText([IntPtr]$edit, [string]$text) {
    foreach ($ch in $text.ToCharArray()) {
        [U]::PostMessage($edit, 0x0102, [IntPtr][int][char]$ch, [IntPtr]1) | Out-Null
        Start-Sleep -Milliseconds 60
    }
    Start-Sleep -Milliseconds 400
}

$proc = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Milliseconds 2200
$proc.Refresh()
$hwnd = $proc.MainWindowHandle
[U]::ShowWindow($hwnd, 3) | Out-Null
[U]::SetForegroundWindow($hwnd) | Out-Null
[U]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null
Start-Sleep -Milliseconds 250
[U]::SetWindowPos($hwnd, [IntPtr](-2), 0, 0, 0, 0, 0x0001 -bor 0x0002) | Out-Null
Start-Sleep -Milliseconds 700

$cr = New-Object U+RECT
[U]::GetClientRect($hwnd, [ref]$cr) | Out-Null
$cw = $cr.Right
Write-Host "client width: $cw"

Shot $hwnd "build\ui_1_dark.png"

# светлая тема
Click $hwnd ($cw - 38) 24
Shot $hwnd "build\ui_2_light.png"

# переходим на «Утренние» (у раздела есть церковнославянский текст)
Click $hwnd 120 157
Start-Sleep -Milliseconds 400

# церковнославянский
Click $hwnd ($cw - 261) 24
Shot $hwnd "build\ui_3_cs.png"

# обратно в русский
Click $hwnd ($cw - 303) 24

# поиск: печатаем в поле
$edit = [U]::GetDlgItem($hwnd, 1001)
$word = -join ([char]0x043F, [char]0x0441, [char]0x0430, [char]0x043B, [char]0x043E, [char]0x043C)
TypeText $edit $word
Shot $hwnd "build\ui_4_search.png"

# очистка поиска, переход в Псалтирь (пункт сайдбара на y=255 client)
Click $hwnd 120 255
Shot $hwnd "build\ui_5_psaltir.png"

Stop-Process -Id $proc.Id -Force
Write-Host "done"
