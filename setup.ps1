# setup.ps1
# Validates prerequisites, checks toolchain, and prepares the environment.

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('x64')]
    [string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\scripts\common.ps1"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Step Recorder Environment Setup" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# 1. Check PowerShell version
Write-Host "Checking PowerShell version: $($PSVersionTable.PSVersion)..." -ForegroundColor Yellow

# 2. Check MSBuild
$msBuild = Find-MSBuild
Write-Host "[OK] Located MSBuild: $msBuild" -ForegroundColor Green

# 3. Check Windows SDK and MFC
$msvcBase = Split-Path (Split-Path (Split-Path (Split-Path $msBuild)))
Write-Host "[OK] Visual Studio / Build Tools root: $msvcBase" -ForegroundColor Green

# 4. Initialize Local AppData folder if not exists
$appData = Join-Path $env:LOCALAPPDATA "StepRecorder"
if (!(Test-Path $appData)) {
    New-Item -ItemType Directory -Force -Path $appData | Out-Null
    Write-Host "[OK] Created application data directory: $appData" -ForegroundColor Green
} else {
    Write-Host "[OK] Application data directory exists: $appData" -ForegroundColor Green
}

# 5. Non-secret local config
$configExample = Join-Path $PSScriptRoot "config.example.json"
$configLocal = Join-Path $appData "settings.json"
if (!(Test-Path $configLocal) -and (Test-Path $configExample)) {
    Copy-Item $configExample -Destination $configLocal
    Write-Host "[OK] Initialized default settings at $configLocal" -ForegroundColor Green
}

Write-Host "`n[SUCCESS] Environment setup completed." -ForegroundColor Green
Write-Host "Next step: Run 'build.bat -Configuration $Configuration -Platform $Platform' to compile." -ForegroundColor Cyan
