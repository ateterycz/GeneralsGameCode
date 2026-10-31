Command & Conquer Generals Zero Hour - community build by {OWNER}
========================================================

This is a modified build of the Zero Hour game engine from the GeneralsGameCode project
(https://github.com/TheSuperHackers/GeneralsGameCode). It is not the original game and is not
made or supported by Electronic Arts.

Version: {VERSION}
Source:  {SOURCE_URL}
License: GPL-3.0 with additional terms, see LICENSE.md


What you need
-------------
1. Your own copy of Command & Conquer Generals Zero Hour (Steam, EA App or CD), started at least
   once. This package contains no game data; the game data comes from your install.
2. The Microsoft Visual C++ 2015-2022 Redistributable, x86 (32-bit) version:
   https://aka.ms/vs/17/release/vc_redist.x86.exe
   If the game says VCRUNTIME140.dll or MSVCP140.dll is missing, install this.


Install
-------
1. Unzip this folder anywhere you like, for example %LOCALAPPDATA%\ZH-{OWNER}.
   Do not copy anything into the game folder. This build leaves your normal game untouched.
2. Double-click Launch-ZH.bat.
   It finds your game folder and starts this build with your game's data.
   Windows may warn "Windows protected your PC" because the exe is not signed. Click
   "More info", then "Run anyway". Compare the checksum below if you want to be sure the file
   is the one that was shared.

To play in a window, add options: Launch-ZH.bat -win -xres 1280 -yres 720
(make a shortcut to Launch-ZH.bat and add them after the path in the shortcut's Target).

If your game folder is not found, create a file named gamedir.txt next to Launch-ZH.bat that
contains the full path of the game folder (the folder that holds WindowZH.big).


Shockwave Chaos mod (optional)
------------------------------
Get the mod from its official download (or GenLauncher), then put its archives in:
  Documents\Command and Conquer Generals Zero Hour Data\Mods\Shockwave Chaos
GenLauncher stores them as .gib files; copy them there and rename .gib to .big. Copy its Data
folder (movies) there too. Then start Launch-ZH-Shockwave.bat.


Multiplayer
-----------
Everyone in a match needs this exact generalszh.exe, and the same mod files if a mod is used.
The game refuses players whose exe or data differ. You cannot play against people who use the
normal game or a different build. Play over LAN, or over a VPN such as Radmin VPN or ZeroTier
for internet play.


Uninstall
---------
Delete this folder. Settings, saves and replays stay in
Documents\Command and Conquer Generals Zero Hour Data, shared with your normal game.


Logs
----
Each session writes a log to Documents\Command and Conquer Generals Zero Hour Data\Logs.
If something goes wrong, send the newest Session_*.txt file.


Checksum (SHA256)
-----------------
See SHA256SUMS.txt. In PowerShell: Get-FileHash .\generalszh.exe
