# Regression: command palette (Ctrl+K), focus mode (F11), search clear, 1120 layout.
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class PT {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int hh, bool repaint);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll", EntryPoint="PostMessageW")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder t, int m);
  [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
function Shot([string]$file) {
    $r = New-Object PT+RECT
    [PT]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
    $g.Dispose()
    $bmp.Save((Join-Path $PWD "build\\$file"), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "  $file ($($r.Right-$r.Left)x$($r.Bottom-$r.Top))"
}
function ClickAt([int]$cx, [int]$cy) {
    $r = New-Object PT+RECT
    [PT]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    [PT]::SetCursorPos(($r.Left + $cx), ($r.Top + $cy)) | Out-Null
    Start-Sleep -Milliseconds 140
    [PT]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 70
    [PT]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 500
}
function PostKey([int]$vk) {
    [PT]::PostMessage($hwnd, 0x0100, [IntPtr]$vk, [IntPtr]0) | Out-Null
    Start-Sleep -Milliseconds 50
    [PT]::PostMessage($hwnd, 0x0101, [IntPtr]$vk, [IntPtr]0) | Out-Null
    Start-Sleep -Milliseconds 350
}
function CtrlK {
    [PT]::PostMessage($hwnd, 0x0100, [IntPtr]0x11, [IntPtr]0) | Out-Null
    [PT]::PostMessage($hwnd, 0x0100, [IntPtr]0x4B, [IntPtr]0) | Out-Null
    [PT]::PostMessage($hwnd, 0x0101, [IntPtr]0x4B, [IntPtr]0) | Out-Null
    [PT]::PostMessage($hwnd, 0x0101, [IntPtr]0x11, [IntPtr]0) | Out-Null
    Start-Sleep -Milliseconds 600
}
function TypeMain([string]$text) {
    foreach ($ch in $text.ToCharArray()) {
        [PT]::PostMessage($hwnd, 0x0102, [IntPtr][int][char]$ch, [IntPtr]1) | Out-Null
        Start-Sleep -Milliseconds 45
    }
    Start-Sleep -Milliseconds 350
}
function TypeEdit([string]$text) {
    foreach ($ch in $text.ToCharArray()) {
        [PT]::PostMessage($edit, 0x0102, [IntPtr][int][char]$ch, [IntPtr]1) | Out-Null
        Start-Sleep -Milliseconds 55
    }
    Start-Sleep -Milliseconds 400
}

Get-Process OrthodoxPrayers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400
Start-Process -FilePath .\build\OrthodoxPrayers.exe | Out-Null
Start-Sleep -Milliseconds 2400
$proc = Get-Process OrthodoxPrayers | Select-Object -First 1
$hwnd = $proc.MainWindowHandle
[PT]::MoveWindow($hwnd, 182, 182, 1240, 860, $true) | Out-Null
[PT]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 900
$edit = [PT]::GetDlgItem($hwnd, 1001)

# 1) Ctrl+K from the main window
CtrlK
Shot "palette_open.png"

# 2) type a filter
TypeMain (-join ([char]0x043A,[char]0x0430,[char]0x043B,[char]0x0435,[char]0x043D))
Shot "palette_filter.png"
PostKey 0x0D
Start-Sleep -Milliseconds 700
Shot "palette_after_enter.png"

# 3) Ctrl+K from the search field (edit subclass)
ClickAt 830 55
TypeEdit (-join ([char]0x043F,[char]0x0441,[char]0x0430,[char]0x043B,[char]0x043E,[char]0x043C))
Start-Sleep -Milliseconds 500
Shot "search_with_text.png"
[PT]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 80
[PT]::keybd_event(0x4B, 0, 0, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 80
[PT]::keybd_event(0x4B, 0, 2, [UIntPtr]::Zero)
[PT]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 700
Shot "palette_from_edit.png"
TypeMain (-join ([char]0x043D,[char]0x0430,[char]0x0441,[char]0x0442,[char]0x0440,[char]0x043E,[char]0x0439))
Shot "palette_settings_filter.png"
PostKey 0x0D
Start-Sleep -Milliseconds 700
Shot "palette_settings_open.png"
PostKey 0x1B
Start-Sleep -Milliseconds 500

# 4) search clear button
# clear the search text left from the previous step
ClickAt 884 55
Start-Sleep -Milliseconds 600
ClickAt 830 55
TypeEdit (-join ([char]0x043F,[char]0x0441,[char]0x0430,[char]0x043B,[char]0x043E,[char]0x043C))
Start-Sleep -Milliseconds 500
Shot "search_clear_before.png"
ClickAt 884 55
Start-Sleep -Milliseconds 600
Shot "search_clear_after.png"
$buf = New-Object System.Text.StringBuilder 128
[PT]::GetWindowText($edit, $buf, 128) | Out-Null
Write-Host "  edit text after clear: '$($buf.ToString())'"

# 5) focus mode from the main window
PostKey 0x7A
Start-Sleep -Milliseconds 700
Shot "palette_focus.png"
PostKey 0x7A
Start-Sleep -Milliseconds 700

# 6) help panel
PostKey 0x70
Start-Sleep -Milliseconds 700
Shot "help_panel.png"
PostKey 0x1B
Start-Sleep -Milliseconds 500

# 7) contents panel at 1240
ClickAt 1100 55
Start-Sleep -Milliseconds 700
Shot "contents_panel.png"
PostKey 0x1B
Start-Sleep -Milliseconds 500

# 8) minimum width 1120
[PT]::MoveWindow($hwnd, 182, 182, 1120, 760, $true) | Out-Null
Start-Sleep -Milliseconds 900
Shot "main_1120.png"
ClickAt 1020 55
Start-Sleep -Milliseconds 700
Shot "settings_1120.png"
PostKey 0x1B
Start-Sleep -Milliseconds 500
ClickAt 976 55
Start-Sleep -Milliseconds 700
Shot "contents_1120.png"
PostKey 0x1B
Start-Sleep -Milliseconds 500

# 9) both sidebar states at 1120 (all six quick tabs must stay visible)
ClickAt 250 71
Start-Sleep -Milliseconds 700
Shot "main_1120_collapsed.png"
ClickAt 42 71
Start-Sleep -Milliseconds 700
Shot "main_1120_expanded.png"

# 10) palette action «Искать ...» (fallback to the main search)
CtrlK
TypeMain (-join ([char]0x043F,[char]0x0441,[char]0x0430,[char]0x043B,[char]0x043E,[char]0x043C))
Start-Sleep -Milliseconds 500
Shot "palette_search_item.png"
PostKey 0x0D
Start-Sleep -Milliseconds 800
Shot "palette_search_result.png"
$buf = New-Object System.Text.StringBuilder 128
[PT]::GetWindowText($edit, $buf, 128) | Out-Null
Write-Host "  edit text after palette search: '$($buf.ToString())'"

# close
[PT]::PostMessage($hwnd, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1100
$proc.Refresh()
Write-Host "  exited: $($proc.HasExited)"
if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
Write-Host "done"
