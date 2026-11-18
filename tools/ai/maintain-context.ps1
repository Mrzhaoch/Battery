[CmdletBinding()]
param(
    [string]$Path = (Get-Location).Path,
    [ValidateRange(1, 100)][int]$KeepFocus = 5,
    [switch]$DryRun,
    [switch]$Force
)
$entry = Join-Path $env:USERPROFILE '.config-kit/context/maintain-context.ps1'
if (-not (Test-Path -LiteralPath $entry)) { throw 'Install the Config kit global notes tools first.' }
& $entry @PSBoundParameters
