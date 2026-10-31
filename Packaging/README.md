# Packaging a build for friends

This folder makes a zip of the Zero Hour build that friends can unzip and start without copying anything into their game
folder. The zip holds no game data: each friend needs their own copy of Zero Hour (Steam, EA App or CD).

## Make the package

1. Commit your changes, then build Zero Hour (the `z_generals` target of the `win32` preset, Release).
2. From the repository folder, run:

   ```powershell
   powershell -ExecutionPolicy Bypass -File Packaging\package.ps1
   ```

The zip lands in `build\friends\ZH-<owner>-<date>-<commit>.zip`. The script stops if the tree has uncommitted changes or
the executable was built from another commit, so the zip always matches its source. `-AllowDirty` skips that for a test
package, and `-Exe` and `-OutDir` change the paths.

Share the zip as a file or attach it to a GitHub Release. Don't commit it: it's a build output.

## What's in it

| File | Purpose |
|---|---|
| `generalszh.exe` | The game |
| `Launch-ZH.bat` | Finds the game folder in the registry (`InstallPath` under `SOFTWARE\Electronic Arts\EA Games\Command and Conquer Generals Zero Hour`, which Steam, the EA App and the CD version write) and starts the game with `-setCwd <game folder>`. Extra options are passed on. A `gamedir.txt` next to it overrides the folder. |
| `Launch-ZH-Shockwave.bat` | The same with `-mod "Mods\Shockwave Chaos"` |
| `README.txt` | Instructions for friends, from `Packaging\README.txt` with the version and source link filled in |
| `LICENSE.md` | The license, which the GPL requires next to the binary |
| `SHA256SUMS.txt` | Checksums of the files |

Friends also need the Microsoft Visual C++ 2015-2022 Redistributable (x86), because the build links the C++ runtime
dynamically. `README.txt` links it.

## Notes

- For multiplayer, everyone needs the same `generalszh.exe`. The window title shows its `exe:` checksum.
- Mods are third-party content. Friends get them from the mod's own download; the package only has the launcher.
- Don't use the CMake install target for this. It copies into the game folder and replaces retail DLLs there.
