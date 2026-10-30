$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('cottage-build-'+[guid]::NewGuid().ToString('N'))
$global:buildCalls=[Collections.Generic.List[string]]::new()
function global:idf.py {
    $global:buildCalls.Add((Get-Location).Path+' | '+($args -join ' '))
    $global:LASTEXITCODE=0
}
function Require([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }
try {
    New-Item -ItemType Directory -Path $fixture,(Join-Path $fixture 'controller'),(Join-Path $fixture 'touchscreen') | Out-Null
    Copy-Item (Join-Path $repo 'tools') $fixture -Recurse
    Copy-Item (Join-Path $repo 'VERSION') $fixture
    $controller=Join-Path $fixture 'controller/sdkconfig.supermini'
    $room=Join-Path $fixture 'touchscreen/sdkconfig.4b'
    Set-Content $controller "CONFIG_THERMO_SERVO_GPIO=5`nCONFIG_THERMO_WIFI_PASSWORD=fixture-private"
    Set-Content $room "CONFIG_ROOM_SENSOR=2`nCONFIG_ROOM_SDA=47`nCONFIG_ROOM_SCL=48`nCONFIG_ROOM_WIFI_PASSWORD=fixture-private"
    Set-Content (Join-Path $fixture 'touchscreen/secrets.h') 'fixture-private'
    $entry=Join-Path $fixture 'tools/build.ps1'
    & $entry
    Require ($global:buildCalls.Count -eq 2) 'Default build must build both production boards exactly once'
    Require ($global:buildCalls[0] -match 'build-supermini') 'Controller must use Super Mini profile'
    Require ($global:buildCalls[1] -match 'build-4b') 'Thermostat must use 4B profile'
    Require (-not (($global:buildCalls -join ' ') -match '\bflash\b')) 'Default build must not flash'
    $config=Get-Content $room -Raw
    Require ($config -match 'CONFIG_ROOM_WIFI_PASSWORD=fixture-private') 'Saved Wi-Fi must survive'
    Require ($config -match 'CONFIG_ROOM_SENSOR=2') 'Saved sensor choice must survive'
    # After normalization, identical builds must not rewrite SDK configuration.
    & $entry
    $modified=(Get-Item $room).LastWriteTimeUtc
    & $entry
    Require ((Get-Item $room).LastWriteTimeUtc -eq $modified) 'Repeated room build rewrote unchanged config'
    $count=$global:buildCalls.Count
    $rejected=$false
    try { & $entry -Device both -Flash -Port COM17 } catch { $rejected=$true }
    Require ($rejected -and $global:buildCalls.Count -eq $count) 'Flashing both with one port must fail before build'
    $rejected=$false
    try { & $entry -Device controller --Flash --Monitor -Port COM17 } catch { $rejected=$true }
    Require ($rejected -and $global:buildCalls.Count -eq $count) 'Unknown double-dash switches must not silently build'
    & $entry -Device controller -Flash -Monitor -Port COM17
    Require ($global:buildCalls.Count -eq $count+3) 'Controller request must build, flash and monitor'
    Require ($global:buildCalls[$count+1] -match '-p COM17 flash') 'Flash must use requested port'
    Require ($global:buildCalls[$count+2] -match '-p COM17 monitor') 'Monitor must use requested port'
    Write-Host 'PASS: production build routing, private config preservation, stable SDK config, strict flags and per-device flash/monitor'
} finally {
    Remove-Item Function:\idf.py
    Remove-Item $fixture -Recurse -Force
}
