@echo off
rem Copies the Shockwave Chaos mod from GenLauncher into this build's mods folder. See Copy-Shockwave.ps1.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Copy-Shockwave.ps1" %*
pause
