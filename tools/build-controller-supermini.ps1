param([switch]$Flash, [switch]$Monitor, [string]$Port='')
$ErrorActionPreference='Stop'
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    throw 'Run from the VSCode ESP-IDF 6.0.1 PowerShell terminal.'
}
if (($Flash -or $Monitor) -and -not $Port) {
    throw 'Specify -Port with -Flash or -Monitor. The replacement board may have a new COM port.'
}
$projectPath=Join-Path (Split-Path -Parent $PSScriptRoot) 'controller'
Push-Location $projectPath
try {
    # Independent configuration and artifacts; do not overwrite the AtomS3 build.
    $idfArgs=@('-B','build-supermini','-D','SDKCONFIG=sdkconfig.supermini',
        '-D','SDKCONFIG_DEFAULTS=sdkconfig.supermini.defaults')
    # Migrate the original Super Mini GPIO38 build without deleting saved settings.
    $configPath=Join-Path $projectPath 'sdkconfig.supermini'
    if (Test-Path $configPath) {
        $configText=Get-Content $configPath -Raw
        $configText=[regex]::Replace($configText,'(?m)^CONFIG_THERMO_SERVO_GPIO=.*\r?\n?','')
        $configText += [Environment]::NewLine + 'CONFIG_THERMO_SERVO_GPIO=5' + [Environment]::NewLine
        [IO.File]::WriteAllText($configPath,$configText,[Text.UTF8Encoding]::new($false))
    }
    Write-Host 'Super Mini bench: servo signal GPIO5 (outside hole), encoder SDA6/SCL7.'
    & idf.py @idfArgs build
    if ($LASTEXITCODE -ne 0) { throw 'Super Mini controller build failed.' }
    if ($Flash) {
        & idf.py @idfArgs -p $Port flash
        if ($LASTEXITCODE -ne 0) { throw 'Super Mini controller flash failed.' }
    }
    if ($Monitor) {
        & idf.py @idfArgs -p $Port monitor
        if ($LASTEXITCODE -ne 0) { throw 'Super Mini controller monitor failed.' }
    }
} finally { Pop-Location }
