param([ValidateSet('both','controller','touchscreen')][string]$Project='both')
$ErrorActionPreference='Stop'
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    throw 'Run this script from the VSCode ESP-IDF PowerShell terminal.'
}
$repoRoot=Split-Path -Parent $PSScriptRoot
$projects=if ($Project -eq 'both') { @('controller','touchscreen') } else { @($Project) }
foreach ($name in $projects) {
    if ($name -eq 'touchscreen') {
        & (Join-Path $PSScriptRoot 'build-room-4b.ps1')
        continue
    }
    $projectPath=Join-Path $repoRoot $name
    Push-Location $projectPath
    try {
        if (-not (Test-Path sdkconfig)) {
            & idf.py set-target esp32s3
            if ($LASTEXITCODE -ne 0) { throw "Target setup failed for $name" }
        }
        # Keep local Wi-Fi/secrets while migrating the former 1 MiB app slot.
        $configText=Get-Content sdkconfig -Raw
        $configText=[regex]::Replace($configText,'(?m)^(?:CONFIG_PARTITION_TABLE_(?:SINGLE_APP|SINGLE_APP_LARGE|TWO_OTA|CUSTOM|CUSTOM_FILENAME|FILENAME)=.*|# CONFIG_PARTITION_TABLE_(?:SINGLE_APP|SINGLE_APP_LARGE|TWO_OTA|CUSTOM) is not set)\r?\n?','')
        $configText += [Environment]::NewLine + 'CONFIG_PARTITION_TABLE_CUSTOM=y' + [Environment]::NewLine + 'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"' + [Environment]::NewLine
        [IO.File]::WriteAllText((Join-Path (Get-Location).Path 'sdkconfig'),$configText,[Text.UTF8Encoding]::new($false))
        & idf.py build
        if ($LASTEXITCODE -ne 0) { throw "Build failed for $name" }
    } finally { Pop-Location }
}
Write-Host 'Selected firmware builds passed. No devices were flashed.'
