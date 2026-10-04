# scripts/package.ps1
# Creates a standalone release package under dist/.

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('x64')]
    [string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$binDir = Join-Path $repoRoot "build\bin\$Platform\$Configuration"
$appExe = Join-Path $binDir "StepRecorder.exe"

# 1. Ensure build exists
if (!(Test-Path $appExe)) {
    Write-Host "[INFO] Binary not found. Building $Configuration $Platform..." -ForegroundColor Yellow
    & "$PSScriptRoot\build.ps1" -Configuration $Configuration -Platform $Platform
}

# 2. Stage release files
$distRoot = Join-Path $repoRoot "dist"
$packageName = "StepRecorder-$Configuration-$Platform"
$stageDir = Join-Path $distRoot $packageName
$zipOutput = Join-Path $distRoot "$packageName.zip"

if (Test-Path $stageDir) {
    Remove-Item $stageDir -Recurse -Force
}
if (Test-Path $zipOutput) {
    Remove-Item $zipOutput -Force
}

New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Write-Host "Staging release payload to $stageDir..." -ForegroundColor Cyan

Copy-Item $appExe -Destination $stageDir
Copy-Item (Join-Path $repoRoot "README.md") -Destination $stageDir
Copy-Item (Join-Path $repoRoot "THIRD_PARTY_NOTICES.md") -Destination $stageDir
if (Test-Path (Join-Path $repoRoot "config.example.json")) {
    Copy-Item (Join-Path $repoRoot "config.example.json") -Destination $stageDir
}

# 3. Create zip archive
Write-Host "Creating archive: $zipOutput..." -ForegroundColor Cyan
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($stageDir, $zipOutput)

Write-Host "[SUCCESS] Package created successfully: $zipOutput" -ForegroundColor Green
