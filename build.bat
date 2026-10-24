@echo off
rem Build script for "Orthodox Prayers" (MSVC x64).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
if errorlevel 1 pause
