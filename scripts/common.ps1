# scripts/common.ps1
# Common utilities for build, test, run, and packaging automation.

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-RepoRoot {
    $scriptDir = Split-Path -Parent $PSScriptRoot
    return (Resolve-Path $scriptDir).Path
}

function Find-MSBuild {
    # 1. Check if msbuild is already on PATH
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    # 2. Check via vswhere if available
    $vswherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswherePath) {
        $vsInstall = & $vswherePath -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($vsInstall -and (Test-Path $vsInstall)) {
            $candidate = Join-Path $vsInstall "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    # 3. Check known standard Visual Studio / Build Tools locations
    $knownLocations = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($loc in $knownLocations) {
        if (Test-Path $loc) {
            return $loc
        }
    }

    throw "MSBuild could not be located. Please install Visual Studio or Visual Studio Build Tools with C++ Desktop workflow."
}

function Invoke-CheckedNativeTool {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Executable,
        [string[]] $Arguments = @()
    )

    Write-Host "Executing: $Executable $($Arguments -join ' ')" -ForegroundColor Cyan
    & $Executable @Arguments
    $toolExitCode = $LASTEXITCODE
    if ($toolExitCode -ne 0) {
        throw "Native tool failed with exit code ${toolExitCode}: $Executable"
    }
}
