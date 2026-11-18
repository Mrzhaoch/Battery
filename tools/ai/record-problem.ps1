[CmdletBinding()]
param(
    [string]$ProjectRoot = (Get-Location).Path,
    [Parameter(Mandatory = $true)][string]$Slug,
    [string]$Issue = '',
    [string]$Area = '',
    [Parameter(Mandatory = $true)][string]$Attempt,
    [Parameter(Mandatory = $true)][ValidateSet('failed', 'confirmed', 'inconclusive')][string]$Outcome,
    [Parameter(Mandatory = $true)][string]$Evidence,
    [string]$Commit = ''
)
$entry = Join-Path $env:USERPROFILE '.config-kit/context/record-problem.ps1'
if (-not (Test-Path -LiteralPath $entry)) { throw 'Install the Config kit global notes tools first.' }
& $entry @PSBoundParameters
