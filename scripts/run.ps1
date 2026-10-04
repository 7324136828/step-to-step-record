# scripts/run.ps1
# Launches StepRecorder desktop application or self-test.

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$appExe = Join-Path $repoRoot "build\bin\$Platform\$Configuration\StepRecorder.exe"

if (!(Test-Path $appExe)) {
    Write-Host "[INFO] Binary not found. Building $Configuration $Platform..." -ForegroundColor Yellow
    & "$PSScriptRoot\build.ps1" -Configuration $Configuration -Platform $Platform
}

if ($SelfTest) {
    Write-Host "Running StepRecorder in self-test mode..." -ForegroundColor Cyan
    Invoke-CheckedNativeTool -Executable $appExe -Arguments @('--selftest')
} else {
    Write-Host "Launching StepRecorder desktop application..." -ForegroundColor Green
    Start-Process -FilePath $appExe
}
