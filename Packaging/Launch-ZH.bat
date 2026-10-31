@echo off
setlocal
rem Starts this build of Zero Hour with the data of your own Zero Hour install. Nothing is copied into the game folder.
rem Extra options are passed on to the game, for example: Launch-ZH.bat -win -xres 1280 -yres 720

set "GAMEDIR="
if exist "%~dp0gamedir.txt" set /p GAMEDIR=<"%~dp0gamedir.txt"
if not defined GAMEDIR call :findgame
if not defined GAMEDIR goto notfound
if "%GAMEDIR:~-1%"=="\" set "GAMEDIR=%GAMEDIR:~0,-1%"
if not exist "%GAMEDIR%\WindowZH.big" goto notfound

start "" /D "%GAMEDIR%" "%~dp0generalszh.exe" -setCwd "%GAMEDIR%" %*
exit /b 0

:findgame
rem Steam, the EA App and the CD version all write the game folder here.
for %%v in (32 64) do (
  for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\Electronic Arts\EA Games\Command and Conquer Generals Zero Hour" /v InstallPath /reg:%%v 2^>nul ^| find "InstallPath"') do if not defined GAMEDIR set "GAMEDIR=%%b"
)
for /f "tokens=2,*" %%a in ('reg query "HKCU\SOFTWARE\Electronic Arts\EA Games\Command and Conquer Generals Zero Hour" /v InstallPath 2^>nul ^| find "InstallPath"') do if not defined GAMEDIR set "GAMEDIR=%%b"
exit /b 0

:notfound
echo Could not find your Command ^& Conquer Generals Zero Hour folder.
echo Start the game once from Steam or the EA App, or create a file named gamedir.txt next to this
echo script that contains the path of the game folder (the folder that holds WindowZH.big).
pause
exit /b 1