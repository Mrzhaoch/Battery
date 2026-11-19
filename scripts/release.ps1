# One command for a release, run on your own PC from an up-to-date `main`:
#   powershell -ExecutionPolicy Bypass -File scripts\release.ps1 -Version 1.2.3 [-NotesFile notes.md]
# It tags the current main, pushes the tag, waits for CI to build, test and publish the release, checks
# SHA256SUMS.txt against the files GitHub serves, signs it with the release key (scripts\sign-release.ps1),
# checks the signature against the key built into the app, uploads SHA256SUMS.txt.sig and, with
# -NotesFile, sets the release notes. The key never leaves this PC.
# Key password: typed when asked, or read from -PasswordFile if that file exists (a plain-text file you
# create yourself; anyone who can read it and the key file can sign releases, so keep both private).
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$NotesFile,
    [string]$KeyFile = (Join-Path $env:USERPROFILE '.wirelessbattery\release-key.wbk'),
    [string]$PasswordFile = (Join-Path $env:USERPROFILE '.wirelessbattery\release-key.pass')
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$repo = 'ahizechi/Battery'
$tag = "v$Version"
Import-Module (Join-Path $PSScriptRoot 'release-key.psm1') -Force

function Step([string]$text) { Write-Host ''; Write-Host "== $text" }
# Named Invoke-*, not Git / Gh: PowerShell finds functions before programs, so `& git` inside a function
# named "Git" would call that function again.
function Invoke-Git { & git.exe -C $root @args; if ($LASTEXITCODE -ne 0) { throw "git $($args -join ' ') failed" } }
function Invoke-Gh { $out = & gh.exe @args; if ($LASTEXITCODE -ne 0) { throw "gh $($args -join ' ') failed" }; $out }

if ($Version -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$') { throw 'Version must be MAJOR.MINOR.PATCH, for example 1.2.3.' }
if (-not (Test-Path -LiteralPath $KeyFile)) { throw "No release key at $KeyFile (create one with scripts\new-release-key.ps1)." }
if ($NotesFile -and -not (Test-Path -LiteralPath $NotesFile)) { throw "Notes file not found: $NotesFile" }
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { throw 'The GitHub CLI (gh) is needed and signed in.' }

Step "Checking that main is clean, current and green"
if ((Invoke-Git branch --show-current) -ne 'main') { throw 'Switch to main first.' }
if (Invoke-Git status --porcelain) { throw 'Uncommitted changes: commit or stash them first.' }
Invoke-Git fetch -q origin --tags
$head = Invoke-Git rev-parse HEAD
if ($head -ne (Invoke-Git rev-parse origin/main)) { throw 'Local main differs from GitHub main: pull or push first.' }
if (Invoke-Git tag --list $tag) { throw "Tag $tag already exists." }
$ci = Invoke-Gh run list -R $repo -b main -c $head -w build -L 1 --json conclusion,status | ConvertFrom-Json
if (-not $ci -or $ci[0].conclusion -ne 'success') { throw "CI for main ($($head.Substring(0,7))) has not passed yet; wait for it, then run this again." }
Write-Host "main $($head.Substring(0,7)) is clean, pushed and its CI passed."

Step "Tagging $tag and pushing it"
Invoke-Git tag -a $tag -m "Battery $Version"
Invoke-Git push -q origin $tag

Step "Waiting for CI to build, test and publish the release (about 10 minutes)"
$run = $null
for ($i = 0; $i -lt 40 -and -not $run; $i++) {
    Start-Sleep -Seconds 5
    $run = (Invoke-Gh run list -R $repo -b $tag -w build -L 1 --json databaseId | ConvertFrom-Json) | Select-Object -First 1
}
if (-not $run) { throw "No CI run started for $tag." }
& gh run watch $run.databaseId -R $repo -i 30 --exit-status | Out-Null
if ($LASTEXITCODE -ne 0) { throw "The release build failed: https://github.com/$repo/actions/runs/$($run.databaseId)" }

Step "Checking SHA256SUMS.txt against the files GitHub serves"
$work = Join-Path $env:TEMP "battery-release-$Version"
if (Test-Path $work) { Remove-Item -LiteralPath $work -Recurse -Force }
New-Item -ItemType Directory -Path $work | Out-Null
Invoke-Gh release download $tag -R $repo -p SHA256SUMS.txt -D $work | Out-Null
$sums = Join-Path $work 'SHA256SUMS.txt'
$assets = (Invoke-Gh api "repos/$repo/releases/tags/$tag" | ConvertFrom-Json).assets
$listed = 0
foreach ($line in (Get-Content -LiteralPath $sums)) {
    if ($line -notmatch '^([0-9a-fA-F]{64}) [ *](.+?)\s*$') { continue }
    $asset = $assets | Where-Object name -eq $Matches[2]
    if (-not $asset -or $asset.digest -ne "sha256:$($Matches[1].ToLower())") { throw "$($Matches[2]) does not match the checksum list." }
    Write-Host "  OK  $($Matches[2])"
    $listed++
}
if ($listed -lt 2) { throw 'SHA256SUMS.txt lists fewer files than expected.' }

Step "Signing the checksum list"
$sig = Join-Path $work 'SHA256SUMS.txt.sig'
$signArgs = @{ Version = $Version; KeyFile = $KeyFile; SumsFile = $sums; OutFile = $sig; Yes = $true }
if (Test-Path -LiteralPath $PasswordFile) {
    $signArgs.Password = ConvertTo-SecureString ((Get-Content -LiteralPath $PasswordFile -Raw).Trim()) -AsPlainText -Force
    Write-Host "Using the password in $PasswordFile."
}
& (Join-Path $PSScriptRoot 'sign-release.ps1') @signArgs
$public = [regex]::Match([IO.File]::ReadAllText((Join-Path $root 'src\update_public_key.h')), '"([A-Za-z0-9+/=]{40,})"').Groups[1].Value
$signature = [Convert]::FromBase64String((Get-Content -LiteralPath $sig -Raw).Trim())
if (-not (Test-ReleaseSignature ([IO.File]::ReadAllBytes($sums)) $signature $public)) { throw 'The signature does not match the key built into the app.' }

Step "Uploading the signature$(if ($NotesFile) { ' and the release notes' })"
Invoke-Gh release upload $tag $sig -R $repo --clobber | Out-Null
if ($NotesFile) { Invoke-Gh release edit $tag -R $repo --title "Battery $Version" --notes-file $NotesFile | Out-Null }
else { Invoke-Gh release edit $tag -R $repo --title "Battery $Version" | Out-Null }
Remove-Item -LiteralPath $work -Recurse -Force

Write-Host ''
Write-Host "Released and signed: https://github.com/$repo/releases/tag/$tag"
Write-Host 'Installed copies offer it at their next daily update check.'
