# Builds the zip that friends unzip and start with Launch-ZH.bat, from an already built generalszh.exe.
# The exe must be built from the current commit with no uncommitted changes, so that the package matches its source.
#
# Usage, from the repository folder after building Zero Hour:
#   powershell -ExecutionPolicy Bypass -File Packaging\package.ps1
param(
	[string]$Exe = '',
	[string]$OutDir = '',
	[switch]$AllowDirty
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $Exe) { $Exe = Join-Path $root 'build\win32\GeneralsMD\Release\generalszh.exe' }
if (-not $OutDir) { $OutDir = Join-Path $root 'build\friends' }

function Fail([string]$message) { Write-Host $message -ForegroundColor Red; exit 1 }

if (-not (Test-Path -LiteralPath $Exe)) { Fail "No game executable at $Exe. Build the z_generals target first." }

$commit = (& git -C $root rev-parse --short=9 HEAD).Trim()
if ($LASTEXITCODE -ne 0) { Fail 'Could not read the current commit with git.' }
$dirty = & git -C $root status --porcelain --untracked-files=no
if ($dirty -and -not $AllowDirty) { Fail "The working tree has uncommitted changes, so the package would not match commit $commit. Commit them, or pass -AllowDirty for a test package." }

# The build stamps its version, which ends in the commit, into the executable. It must be the current commit.
$exeText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($Exe))
$versionMatch = [regex]::Match($exeText, "~?[A-Za-z0-9.\-]*-g$commit")
if (-not $versionMatch.Success) { Fail "$Exe was not built from commit $commit. Build it again, then run this script." }
$version = $versionMatch.Value
if ($version.StartsWith('~') -and -not $AllowDirty) { Fail "$Exe was built with uncommitted changes. Build it again from a clean tree." }

# The owner and the source link come from the origin remote, for example https://github.com/owner/GeneralsGameCode.git.
$origin = (& git -C $root remote get-url origin).Trim()
$repoMatch = [regex]::Match($origin, 'github\.com[:/]([^/]+)/([^/]+?)(\.git)?$')
if (-not $repoMatch.Success) { Fail "The origin remote $origin is not a GitHub repository." }
$owner = $repoMatch.Groups[1].Value
$sourceUrl = "https://github.com/$owner/$($repoMatch.Groups[2].Value)/tree/$commit"

$name = "ZH-$owner-$(Get-Date -Format 'yyyy-MM-dd')-$commit"
$stage = Join-Path $OutDir $name
$zip = "$stage.zip"
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

# Text files get Windows line endings, which cmd needs for the launchers, whatever git checked out.
function Write-TextFile([string]$path, [string]$text) {
	[IO.File]::WriteAllText($path, ($text -replace "`r?`n", "`r`n"), [Text.Encoding]::ASCII)
}

Copy-Item -LiteralPath $Exe -Destination $stage
foreach ($script in 'Launch-ZH.bat', 'Launch-ZH-Shockwave.bat', 'Copy-Shockwave.bat', 'Copy-Shockwave.ps1') {
	Write-TextFile (Join-Path $stage $script) ([IO.File]::ReadAllText((Join-Path $PSScriptRoot $script)))
}
$readme = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'README.txt'))
$readme = $readme.Replace('{OWNER}', $owner).Replace('{VERSION}', $version).Replace('{SOURCE_URL}', $sourceUrl)
Write-TextFile (Join-Path $stage 'README.txt') $readme
Write-TextFile (Join-Path $stage 'LICENSE.md') ([IO.File]::ReadAllText((Join-Path $root 'LICENSE.md')))

$sums = Get-ChildItem -LiteralPath $stage -File | Sort-Object Name | ForEach-Object {
	'{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name
}
Write-TextFile (Join-Path $stage 'SHA256SUMS.txt') (($sums -join "`n") + "`n")

Compress-Archive -Path $stage -DestinationPath $zip

Write-Host "Package: $zip"
Write-Host "Version: $version"
Write-Host "generalszh.exe SHA256: $((Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash.ToLower())"
