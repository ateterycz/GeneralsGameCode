# Copies the Shockwave Chaos mod that GenLauncher installed into the mods folder of this build, renaming its .gib
# archives to .big, so that Launch-ZH-Shockwave.bat can start it. The GenLauncher copy is left as it is.
#
# Without -Source, it takes the newest version in <game folder>\GLM\Shockwave Chaos Mod. -Source can point at any
# folder with the mod's .gib or .big archives instead, for example one from the mod's own download.
param(
	[string]$Source = ''
)

$ErrorActionPreference = 'Stop'
$modName = 'Shockwave Chaos'

function Fail([string]$message) { Write-Host $message -ForegroundColor Red; exit 1 }

# The same lookup as Launch-ZH.bat: a gamedir.txt next to the scripts, then the game's registry entry.
function Find-GameDir {
	$override = Join-Path $PSScriptRoot 'gamedir.txt'
	if (Test-Path -LiteralPath $override) { return (Get-Content -LiteralPath $override -TotalCount 1).Trim() }
	$key = 'SOFTWARE\Electronic Arts\EA Games\Command and Conquer Generals Zero Hour'
	foreach ($view in [Microsoft.Win32.RegistryView]::Registry32, [Microsoft.Win32.RegistryView]::Registry64) {
		foreach ($hive in [Microsoft.Win32.RegistryHive]::LocalMachine, [Microsoft.Win32.RegistryHive]::CurrentUser) {
			$base = [Microsoft.Win32.RegistryKey]::OpenBaseKey($hive, $view)
			$sub = $base.OpenSubKey($key)
			if ($sub -and $sub.GetValue('InstallPath')) { return ([string]$sub.GetValue('InstallPath')).TrimEnd('\') }
		}
	}
	return ''
}

if (-not $Source) {
	$gameDir = Find-GameDir
	if (-not $gameDir) { Fail 'Could not find your Zero Hour folder. Start the game once, or create gamedir.txt next to this script.' }
	$glm = Join-Path $gameDir "GLM\$modName Mod"
	if (-not (Test-Path -LiteralPath $glm)) { Fail "Shockwave Chaos is not installed in GenLauncher ($glm not found). Install it there, or pass -Source <folder>." }
	$versions = Get-ChildItem -LiteralPath $glm -Directory | Where-Object { Get-ChildItem -LiteralPath $_.FullName -Filter '*.gib' }
	if (-not $versions) { Fail "No Shockwave Chaos version with .gib archives in $glm." }
	$Source = ($versions | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
}
if (-not (Get-ChildItem -LiteralPath $Source -File | Where-Object { $_.Extension -in '.gib', '.big' })) { Fail "$Source has no .gib or .big archives." }

$userData = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Command and Conquer Generals Zero Hour Data'
$target = Join-Path $userData "Mods\$modName"

# An older copy is replaced as a whole, so that no archive of another version stays behind.
if (Test-Path -LiteralPath $target) {
	Write-Host "Replacing the previous copy in $target"
	Remove-Item -LiteralPath $target -Recurse -Force
}
New-Item -ItemType Directory -Force $target | Out-Null

Write-Host "Copying $Source"
Write-Host "     to $target"
$count = 0
foreach ($file in Get-ChildItem -LiteralPath $Source -File -Recurse) {
	$relative = $file.FullName.Substring($Source.TrimEnd('\').Length + 1)
	if ($file.Extension -eq '.gib') { $relative = [IO.Path]::ChangeExtension($relative, '.big') }
	$destination = Join-Path $target $relative
	New-Item -ItemType Directory -Force (Split-Path -Parent $destination) | Out-Null
	Copy-Item -LiteralPath $file.FullName -Destination $destination
	$count++
}

$size = (Get-ChildItem -LiteralPath $target -File -Recurse | Measure-Object Length -Sum).Sum / 1GB
Write-Host ('Done: {0} files, {1:N2} GB. Start Launch-ZH-Shockwave.bat to play.' -f $count, $size) -ForegroundColor Green
