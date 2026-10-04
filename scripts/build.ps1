# scripts/build.ps1
# Builds the StepRecorder solution with MSBuild.

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [ValidateSet('Build', 'Rebuild', 'Clean')]
    [string]$Target = 'Build'
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$solutionPath = Join-Path $repoRoot "cpp\StepRecorder.sln"
$msBuildExe = Find-MSBuild

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Building StepRecorder Solution" -ForegroundColor Cyan
Write-Host " Configuration : $Configuration" -ForegroundColor Cyan
Write-Host " Platform      : $Platform" -ForegroundColor Cyan
Write-Host " Target        : $Target" -ForegroundColor Cyan
Write-Host " Solution      : $solutionPath" -ForegroundColor Cyan
Write-Host " MSBuild       : $msBuildExe" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$buildArgs = @(
    $solutionPath,
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    "/t:$Target",
    "/m",
    "/v:minimal",
    "/nologo"
)

Invoke-CheckedNativeTool -Executable $msBuildExe -Arguments $buildArgs

Write-Host "[SUCCESS] Build completed successfully." -ForegroundColor Green
