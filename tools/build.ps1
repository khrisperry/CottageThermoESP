[CmdletBinding()]
param(
    [ValidateSet('both','controller','thermostat')][string]$Device='both',
    [switch]$Flash, [switch]$Monitor, [string]$Port=''
)
$ErrorActionPreference='Stop'
if (($Flash -or $Monitor) -and ($Device -eq 'both' -or -not $Port)) {
    throw 'For Flash/Monitor select one -Device (controller or thermostat) and its -Port COMxx.'
}
$version=(Get-Content (Join-Path (Split-Path -Parent $PSScriptRoot) 'VERSION') -Raw).Trim()
Write-Host "CottageThermoESP V$version / $Device"
$options=@{Flash=$Flash;Monitor=$Monitor;Port=$Port}
if ($Device -in @('both','controller')) {
    & (Join-Path $PSScriptRoot 'build-controller-supermini.ps1') @options
}
if ($Device -in @('both','thermostat')) {
    & (Join-Path $PSScriptRoot 'build-room-4b.ps1') @options
}
if (-not $Flash -and -not $Monitor) { Write-Host 'Build complete. No devices were flashed.' }
