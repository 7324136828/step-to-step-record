# scripts/test.ps1
# Runs unit tests and application self-test.

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
$testsExe = Join-Path $binDir "Tests.exe"
$appExe = Join-Path $binDir "StepRecorder.exe"

if (!(Test-Path $testsExe)) {
    Write-Host "[INFO] Tests binary not found. Running build..." -ForegroundColor Yellow
    & "$PSScriptRoot\build.ps1" -Configuration $Configuration -Platform $Platform
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Running Automated Test Suite" -ForegroundColor Cyan
Write-Host " Configuration : $Configuration" -ForegroundColor Cyan
Write-Host " Platform      : $Platform" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

if (Get-Process -Name StepRecorder -ErrorAction SilentlyContinue) {
    throw 'Close StepRecorder before running tests so recording history can be isolated safely.'
}

if (-not ('RecorderTestEnvironment' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RecorderTestEnvironment {
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X, Y; }
    [DllImport("user32.dll")] public static extern bool GetPhysicalCursorPos(out Point point);
    [DllImport("user32.dll")] public static extern bool SetPhysicalCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hwnd);
}
'@
}
$originalForeground = [RecorderTestEnvironment]::GetForegroundWindow()
$originalCursor = New-Object RecorderTestEnvironment+Point
$cursorCaptured = [RecorderTestEnvironment]::GetPhysicalCursorPos([ref]$originalCursor)

$verificationDir = Join-Path $repoRoot "build\verification\$Configuration"
New-Item -ItemType Directory -Path $verificationDir -Force | Out-Null

function Invoke-BoundedTest {
    param([string]$Executable, [string[]]$Arguments = @(), [string]$Name)

    $stdoutPath = Join-Path $verificationDir "$Name.stdout.log"
    $stderrPath = Join-Path $verificationDir "$Name.stderr.log"
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $Executable
    $startInfo.Arguments = $Arguments -join ' '
    $startInfo.WorkingDirectory = $verificationDir
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    # Keep the native process handle, including when a fast test exits before
    # PowerShell's Start-Process can obtain its exit code.
    $testProcess = New-Object System.Diagnostics.Process
    $testProcess.StartInfo = $startInfo
    $null = $testProcess.Start()
    $stdoutRead = $testProcess.StandardOutput.ReadToEndAsync()
    $stderrRead = $testProcess.StandardError.ReadToEndAsync()
    try {
        if (!$testProcess.WaitForExit(30000)) {
            throw "$Name did not complete within 30 seconds. A recorder deadlock may have regressed."
        }
        [System.IO.File]::WriteAllText($stdoutPath, $stdoutRead.Result)
        [System.IO.File]::WriteAllText($stderrPath, $stderrRead.Result)
        Get-Content -LiteralPath $stdoutPath,$stderrPath
        if ($testProcess.ExitCode -ne 0) {
            throw "$Name failed with exit code $($testProcess.ExitCode)."
        }
    } finally {
        if (!$testProcess.HasExited) {
            Stop-Process -Id $testProcess.Id -Force
            $testProcess.WaitForExit()
        }
        $testProcess.Dispose()
    }
}

# Existing history tests and the app self-test write through the real HistoryStore.
# Run against an empty history and restore the user's original bytes afterward.
$historyDir = Join-Path $env:LOCALAPPDATA 'StepRecorder'
$historyPath = Join-Path $historyDir 'history.json'
$hadHistory = Test-Path -LiteralPath $historyPath
$originalHistory = if ($hadHistory) { [System.IO.File]::ReadAllBytes($historyPath) } else { $null }
try {
    New-Item -ItemType Directory -Path $historyDir -Force | Out-Null
    [System.IO.File]::WriteAllText($historyPath, '[]')

    Write-Host "`n>>> [1/3] Running Unit Tests ($testsExe)..." -ForegroundColor Yellow
    Invoke-BoundedTest -Executable $testsExe -Name 'unit-tests'

    Write-Host "`n>>> [2/3] Running Integration Self-Test ($appExe --selftest)..." -ForegroundColor Yellow
    Invoke-BoundedTest -Executable $appExe -Arguments @('--selftest') -Name 'selftest'
    $selftestResult = Join-Path $verificationDir 'selftest_result.txt'
    if (Test-Path -LiteralPath $selftestResult) { Get-Content -LiteralPath $selftestResult }

    Write-Host "`n>>> [3/3] Running Dialog Lifecycle Tests..." -ForegroundColor Yellow
    & "$PSScriptRoot\test-lifecycle.ps1" -Executable $appExe -InputMode All

    Write-Host "`n[SUCCESS] All unit, self-test, and lifecycle checks passed successfully!" -ForegroundColor Green
} finally {
    if ($hadHistory) {
        [System.IO.File]::WriteAllBytes($historyPath, $originalHistory)
    } elseif (Test-Path -LiteralPath $historyPath) {
        Remove-Item -LiteralPath $historyPath -Force
    }
    if ($cursorCaptured) {
        [RecorderTestEnvironment]::SetPhysicalCursorPos($originalCursor.X, $originalCursor.Y) | Out-Null
    }
    if ([RecorderTestEnvironment]::IsWindow($originalForeground)) {
        [RecorderTestEnvironment]::SetForegroundWindow($originalForeground) | Out-Null
    }
}
