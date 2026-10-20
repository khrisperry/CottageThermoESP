param([switch]$Configure, [switch]$Flash, [switch]$Monitor, [string]$Port='')
$ErrorActionPreference='Stop'
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
