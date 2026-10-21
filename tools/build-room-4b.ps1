param([switch]$Configure, [Alias('Bme280Test')][switch]$Bmp280Test, [switch]$Sht45, [switch]$Flash, [switch]$Monitor, [string]$Port='')
$ErrorActionPreference='Stop'
if ($Bmp280Test -and $Sht45) { throw 'Select either -Bme280Test/-Bmp280Test or -Sht45, not both.' }
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    throw 'Run from the VSCode ESP-IDF 6.0.1 PowerShell terminal.'
}
if (($Flash -or $Monitor) -and -not $Port) { throw 'Specify -Port COMxx for Flash/Monitor.' }
Push-Location (Join-Path (Split-Path -Parent $PSScriptRoot) 'touchscreen')
try {
    if (-not (Test-Path secrets.h)) {
        Copy-Item secrets.h.example secrets.h
        Write-Host 'Created touchscreen/secrets.h. Add your HA long-lived token there and rebuild.'
    }
    # Separate from the old browser-only sdkconfig: 16 MB flash and octal PSRAM.
    # Credentials stay in this ignored local config, not in defaults or git.
    $idfArgs=@('-B','build-4b','-D','SDKCONFIG=sdkconfig.4b')
    if (-not (Test-Path sdkconfig.4b)) {
        & idf.py @idfArgs set-target esp32s3
        if ($LASTEXITCODE -ne 0) { throw '4B target setup failed' }
    }
    # Migrate older local configs too; defaults do not override saved values.
    $configPath=Join-Path (Get-Location).Path 'sdkconfig.4b'
    $configText=Get-Content $configPath -Raw
    $mainStack=[regex]::Match($configText,'(?m)^CONFIG_ESP_MAIN_TASK_STACK_SIZE=(\d+)\r?$')
    if (-not $mainStack.Success -or [int]$mainStack.Groups[1].Value -lt 16384) {
        if ($mainStack.Success) { $configText=$configText.Replace($mainStack.Value,'CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384') }
        else { $configText += [Environment]::NewLine + 'CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384' + [Environment]::NewLine }
        [IO.File]::WriteAllText($configPath,$configText,[Text.UTF8Encoding]::new($false))
    }
    # Existing sdkconfig values win over defaults: migrate TLS allocations too.
    $configText=Get-Content $configPath -Raw
    $configText=[regex]::Replace($configText,'(?m)^(?:CONFIG_MBEDTLS_(?:INTERNAL|EXTERNAL|DEFAULT|CUSTOM|IRAM_8BIT)_MEM_ALLOC=.*|# CONFIG_MBEDTLS_(?:INTERNAL|EXTERNAL|DEFAULT|CUSTOM|IRAM_8BIT)_MEM_ALLOC is not set)\r?\n?','')
    $configText += [Environment]::NewLine + 'CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y' + [Environment]::NewLine
    [IO.File]::WriteAllText($configPath,$configText,[Text.UTF8Encoding]::new($false))
    # The native Settings volume control requires these widgets in saved configs.
    $configText=Get-Content $configPath -Raw
    foreach ($widget in @('BAR','SLIDER')) {
        $configText=[regex]::Replace($configText,"(?m)^(?:CONFIG_LV_USE_${widget}=.*|# CONFIG_LV_USE_${widget} is not set)\r?\n?",'')
        $configText += [Environment]::NewLine + "CONFIG_LV_USE_${widget}=y" + [Environment]::NewLine
    }
    [IO.File]::WriteAllText($configPath,$configText,[Text.UTF8Encoding]::new($false))
    $configText=Get-Content $configPath -Raw
    $configText=[regex]::Replace($configText,'(?m)^CONFIG_LV_MEM_SIZE_KILOBYTES=.*\r?\n?','')
    $configText += [Environment]::NewLine + 'CONFIG_LV_MEM_SIZE_KILOBYTES=256' + [Environment]::NewLine
    [IO.File]::WriteAllText($configPath,$configText,[Text.UTF8Encoding]::new($false))
    # OTA requires the rollback-capable bootloader on the initial USB flash.
    $configText=Get-Content $configPath -Raw
    $configText=[regex]::Replace($configText,'(?m)^(?:CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=.*|# CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE is not set)\r?\n?','')
    $configText += [Environment]::NewLine + 'CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y' + [Environment]::NewLine
    [IO.File]::WriteAllText($configPath,$configText,[Text.UTF8Encoding]::new($false))
    if ($Bmp280Test -or $Sht45) {
        # Explicit sensor selection replaces a prior BME test or SHT selection.
        $configText=Get-Content $configPath -Raw
        $configText=[regex]::Replace($configText,'(?m)^CONFIG_ROOM_(?:SENSOR|SDA|SCL)=.*\r?\n?','')
        $sensorType=if ($Sht45) { 2 } else { 3 }
        $configText += [Environment]::NewLine + "CONFIG_ROOM_SENSOR=$sensorType`nCONFIG_ROOM_SDA=47`nCONFIG_ROOM_SCL=48`n"
        [IO.File]::WriteAllText($configPath,$configText,[Text.UTF8Encoding]::new($false))
        if ($Sht45) {
            Write-Host 'SHT45/SHT4x primary probe enabled: I2C 0x44, SDA47/SCL48; readings displayed and reported to controller.'
        } else {
            Write-Host 'BMP280/BME280 temperature test enabled: SDA47/SCL48; local display/log only, no controller reports.'
        }
    }
    if ($Configure) {
        & idf.py @idfArgs menuconfig
        if ($LASTEXITCODE -ne 0) { throw '4B configuration failed' }
    }
    & idf.py @idfArgs build
    if ($LASTEXITCODE -ne 0) { throw '4B build failed' }
    if ($Flash) {
        & idf.py @idfArgs -p $Port flash
        if ($LASTEXITCODE -ne 0) { throw '4B flash failed' }
    }
    if ($Monitor) { & idf.py @idfArgs -p $Port monitor }
} finally { Pop-Location }

