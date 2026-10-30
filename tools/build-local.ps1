# Compatibility entry point. Production hardware is Super Mini + Waveshare 4B.
[CmdletBinding()]
param([ValidateSet('both','controller','touchscreen')][string]$Project='both')
$ErrorActionPreference='Stop'
$device=if ($Project -eq 'touchscreen') { 'thermostat' } else { $Project }
& (Join-Path $PSScriptRoot 'build.ps1') -Device $device
