param([string]$BindAddress='127.0.0.1', [int]$Port=8087)
$ErrorActionPreference='Stop'
Push-Location $PSScriptRoot
try {
    if (-not (Test-Path .venv)) { python -m venv .venv; if ($LASTEXITCODE -ne 0) { throw 'Python environment setup failed' } }
    $python=Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
    & $python -m pip install -r requirements.txt
    if ($LASTEXITCODE -ne 0) { throw 'Bridge dependency installation failed' }
    if (-not $env:HA_TOKEN) {
        $secure=Read-Host 'Home Assistant long-lived access token (kept in this process only)' -AsSecureString
        $ptr=[Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
        try { $env:HA_TOKEN=[Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr) }
        finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr) }
    }
    & $python bridge.py --bind $BindAddress --port $Port
    if ($LASTEXITCODE -ne 0) { throw 'Bridge exited with an error' }
} finally {
    Remove-Item Env:HA_TOKEN -ErrorAction SilentlyContinue
    Pop-Location
}
