# Build script for "Orthodox Prayers" (MSVC x64).
# NOTE: keep this file ASCII-only (Windows PowerShell 5.1 reads .ps1 as ANSI without BOM).
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root 'src'
$out = Join-Path $root 'build'

Write-Host '=== Looking for MSVC ===' -ForegroundColor Cyan

$candidates = @()
foreach ($ver in '18', '2022', '2019') {
    foreach ($ed in 'Community', 'BuildTools', 'Professional', 'Enterprise') {
        $candidates += "C:\Program Files\Microsoft Visual Studio\$ver\$ed\VC\Auxiliary\Build\vcvarsall.bat"
        $candidates += "C:\Program Files (x86)\Microsoft Visual Studio\$ver\$ed\VC\Auxiliary\Build\vcvarsall.bat"
    }
}
$vcvars = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $vcvars) {
    Write-Host '[ERROR] Visual Studio with C++ tools was not found.' -ForegroundColor Red
    exit 1
}
Write-Host "vcvarsall: $vcvars"

$dump = cmd /c "`"$vcvars`" x64 >nul && set"
foreach ($line in $dump) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($matches[1])" -Value $matches[2]
    }
}
if (-not (Test-Path -LiteralPath "env:VSINSTALLDIR")) {
    Write-Host '[ERROR] MSVC environment was not initialized.' -ForegroundColor Red
    exit 1
}

if (-not (Test-Path -LiteralPath $out)) { New-Item -ItemType Directory -Path $out | Out-Null }
Push-Location $src

Write-Host '=== Compiling ===' -ForegroundColor Cyan
$clArgs = @(
    '/nologo', '/std:c++17', '/utf-8', '/O2', '/EHsc', '/W3', '/permissive-',
    '/DUNICODE', '/D_UNICODE', '/D_WIN32_WINNT=0x0A00', '/GS', '/Zi',
    "/Fo`"$out\\`"", "/Fd`"$out\vc.pdb`"",
    "/Fe`"$out\OrthodoxPrayers.exe`"",
    'app.cpp', 'prayers_data.cpp', 'calendar_dates.cpp', 'calendar_menaion.cpp',
    'calendar_day.cpp', 'calendar.cpp', 'calendar_kalendar.cpp',
    'calendar_sluzhby.cpp', 'calendar_sluzhby2.cpp', 'calendar_azbyka.cpp',
    'bible_section.cpp', 'pominalnik.cpp', 'pominalnik_dialog.cpp', 'online.cpp',
    '/link', '/SUBSYSTEM:WINDOWS', '/MANIFEST:EMBED',
    "/MANIFESTINPUT:`"app.manifest`"",
    '/MANIFESTDEPENDENCY:"type=''win32'' name=''Microsoft.Windows.Common-Controls'' version=''6.0.0.0'' processorArchitecture=''amd64'' publicKeyToken=''6595b64144ccf1df'' language=''*''"',
    'gdiplus.lib', 'comctl32.lib', 'dwmapi.lib', 'shell32.lib', 'user32.lib', 'gdi32.lib'
)
& cl.exe @clArgs
$code = $LASTEXITCODE
Pop-Location

if ($code -ne 0) {
    Write-Host ''
    Write-Host '[ERROR] Build failed.' -ForegroundColor Red
    exit $code
}

$exe = Join-Path $out 'OrthodoxPrayers.exe'
$size = [math]::Round((Get-Item -LiteralPath $exe).Length / 1KB, 1)
Write-Host ''
Write-Host "Done: $exe  ($size KB)" -ForegroundColor Green
exit 0
