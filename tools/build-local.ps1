param([ValidateSet('both','controller','touchscreen')][string]$Project='both')
$ErrorActionPreference='Stop'
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    throw 'Run this script from the VSCode ESP-IDF PowerShell terminal.'
}
$repoRoot=Split-Path -Parent $PSScriptRoot
$projects=if ($Project -eq 'both') { @('controller','touchscreen') } else { @($Project) }
foreach ($name in $projects) {
    $projectPath=Join-Path $repoRoot $name
    Push-Location $projectPath
    try {
        if (-not (Test-Path sdkconfig)) {
            & idf.py set-target esp32s3
            if ($LASTEXITCODE -ne 0) { throw "Target setup failed for $name" }
        }
        & idf.py build
        if ($LASTEXITCODE -ne 0) { throw "Build failed for $name" }
    } finally { Pop-Location }
}
Write-Host 'Selected firmware builds passed. No devices were flashed.'
