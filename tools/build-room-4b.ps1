param([switch]$Configure, [switch]$Flash, [switch]$Monitor, [string]$Port='')
$ErrorActionPreference='Stop'
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    throw 'Run from the VSCode ESP-IDF 6.0.1 PowerShell terminal.'
}
if (($Flash -or $Monitor) -and -not $Port) { throw 'Specify -Port COMxx for Flash/Monitor.' }
Push-Location (Join-Path (Split-Path -Parent $PSScriptRoot) 'touchscreen')
try {
    # Separate from the old browser-only sdkconfig: 16 MB flash and octal PSRAM.
    # Credentials stay in this ignored local config, not in defaults or git.
    $idfArgs=@('-B','build-4b','-D','SDKCONFIG=sdkconfig.4b')
    if (-not (Test-Path sdkconfig.4b)) {
        & idf.py @idfArgs set-target esp32s3
        if ($LASTEXITCODE -ne 0) { throw '4B target setup failed' }
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
