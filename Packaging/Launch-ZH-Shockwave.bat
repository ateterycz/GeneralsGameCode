@echo off
rem Starts this build with the Shockwave Chaos mod. Put the mod's .big files (renamed from .gib if GenLauncher
rem downloaded them) in Documents\Command and Conquer Generals Zero Hour Data\Mods\Shockwave Chaos first.
call "%~dp0Launch-ZH.bat" -mod "Mods\Shockwave Chaos" %*