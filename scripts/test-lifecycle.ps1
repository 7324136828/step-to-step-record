[CmdletBinding()]
param(
    [string]$Executable = "$PSScriptRoot\..\build\bin\x64\Release\StepRecorder.exe",
    [ValidateSet('Command', 'Real', 'All')]
    [string]$InputMode = 'All'
)

$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
if (Get-Process -Name StepRecorder -ErrorAction SilentlyContinue) {
    throw 'Close existing StepRecorder instances before running lifecycle tests, so their history and recordings remain undisturbed.'
}
$historyPath = Join-Path $env:LOCALAPPDATA 'StepRecorder\history.json'
$historyExisted = Test-Path -LiteralPath $historyPath
$historyBytes = if ($historyExisted) { [System.IO.File]::ReadAllBytes($historyPath) } else { $null }
$outputDirectory = Join-Path $PSScriptRoot "..\build\verification\lifecycle-$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$outputDirectory = (Resolve-Path -LiteralPath $outputDirectory).Path

# Exercise the real modal-dialog routes, including the default WM_CLOSE
# dispatch that previously recursively called OnCancel. Every handle is
# resolved against the process launched by this test, never a user's instance.
if (-not ('RecorderLifecycleTest' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class RecorderLifecycleTest {
    public static string ActivationDetails = "";
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] private struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] private struct MouseInput { public int X, Y; public uint Data, Flags, Time; public UIntPtr ExtraInfo; }
    [StructLayout(LayoutKind.Sequential)] private struct Input { public uint Type; public MouseInput Mouse; }
    private delegate bool EnumWindowProc(IntPtr hwnd, IntPtr data);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowProc callback, IntPtr data);
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumWindowProc callback, IntPtr data);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")] private static extern IntPtr GetWindow(IntPtr hwnd, uint command);
    [DllImport("user32.dll")] private static extern int GetDlgCtrlID(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
    [DllImport("user32.dll")] private static extern IntPtr GetParent(IntPtr hwnd);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder value, int count);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool AttachThreadInput(uint first, uint second, bool attach);
    [DllImport("kernel32.dll")] private static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd, out Rect bounds);
    [DllImport("user32.dll")] private static extern IntPtr WindowFromPoint(Point point);
    [DllImport("user32.dll")] private static extern IntPtr GetAncestor(IntPtr hwnd, uint flags);
    [DllImport("user32.dll", SetLastError=true)] private static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool ShowWindow(IntPtr hwnd, int command);
    [DllImport("user32.dll")] private static extern uint SendInput(uint count, Input[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern IntPtr SendMessageTimeout(IntPtr hwnd, uint message, IntPtr wParam, string lParam, uint flags, uint timeout, out IntPtr result);
    [DllImport("user32.dll", EntryPoint="SendMessageTimeoutW", CharSet=CharSet.Unicode)] private static extern IntPtr SendMessageTimeoutBuffer(IntPtr hwnd, uint message, IntPtr wParam, StringBuilder lParam, uint flags, uint timeout, out IntPtr result);
    public static string Text(IntPtr hwnd) {
        IntPtr result;
        var buffer = new StringBuilder(256);
        return SendMessageTimeoutBuffer(hwnd, 13, (IntPtr)buffer.Capacity, buffer, 2, 250, out result) != IntPtr.Zero ? buffer.ToString() : null;
    }
    public static void Show(IntPtr hwnd) {
        ShowWindow(hwnd, 9);
        Activate(hwnd);
    }
    public static void Activate(IntPtr hwnd) {
        if (SetForegroundWindow(hwnd)) return;
        // A background test shell may be subject to Windows' foreground lock.
        // Temporarily share the foreground input queue while activating only
        // our test window, and always detach before issuing any test input.
        uint foregroundProcess;
        uint foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), out foregroundProcess);
        uint currentThread = GetCurrentThreadId();
        bool attached = foregroundThread != 0 && foregroundThread != currentThread && AttachThreadInput(currentThread, foregroundThread, true);
        try { SetForegroundWindow(hwnd); }
        finally { if (attached) AttachThreadInput(currentThread, foregroundThread, false); }
    }
    public static bool MoveToControl(IntPtr control) {
        Rect bounds;
        if (!GetWindowRect(control, out bounds)) return false;
        var point = new Point { X = (bounds.Left + bounds.Right) / 2, Y = (bounds.Top + bounds.Bottom) / 2 };
        if (!SetCursorPos(point.X, point.Y)) return false;
        IntPtr hit = WindowFromPoint(point);
        uint targetProcess, hitProcess;
        GetWindowThreadProcessId(control, out targetProcess);
        GetWindowThreadProcessId(hit, out hitProcess);
        return targetProcess == hitProcess && GetAncestor(control, 2) == GetAncestor(hit, 2);
    }
    public static bool ActivateByOwnClick(IntPtr hwnd) {
        ActivationDetails = "";
        if (!SetWindowPos(hwnd, (IntPtr)(-1), 0, 0, 0, 0, 0x0013)) {
            ActivationDetails = "SetWindowPos failed: " + Marshal.GetLastWin32Error();
            return false;
        }
        try {
            Rect bounds;
            IntPtr control = Control(hwnd, 1018);
            if (!GetWindowRect(control, out bounds)) { ActivationDetails = "No count-label bounds for control " + control; return false; }
            var point = new Point { X = (bounds.Left + bounds.Right) / 2, Y = (bounds.Top + bounds.Bottom) / 2 };
            if (!SetCursorPos(point.X, point.Y)) return false;
            IntPtr hit = WindowFromPoint(point);
            uint hitProcess, ownProcess;
            GetWindowThreadProcessId(hit, out hitProcess);
            GetWindowThreadProcessId(hwnd, out ownProcess);
            Rect mainBounds;
            GetWindowRect(hwnd, out mainBounds);
            IntPtr foreground = GetForegroundWindow();
            uint foregroundProcess;
            GetWindowThreadProcessId(foreground, out foregroundProcess);
            var foregroundClass = new StringBuilder(64);
            GetClassName(foreground, foregroundClass, foregroundClass.Capacity);
            string foregroundName = "unknown";
            try { foregroundName = System.Diagnostics.Process.GetProcessById((int)foregroundProcess).ProcessName; } catch {}
            ActivationDetails = "own HWND=" + hwnd + " PID=" + ownProcess + " visible=" + IsWindowVisible(hwnd) + " enabled=" + IsWindowEnabled(hwnd) +
                " main bounds=" + mainBounds.Left + "," + mainBounds.Top + "," + mainBounds.Right + "," + mainBounds.Bottom +
                " count bounds=" + bounds.Left + "," + bounds.Top + "," + bounds.Right + "," + bounds.Bottom +
                " hit root=" + GetAncestor(hit, 2) + " PID=" + hitProcess + " foreground HWND=" + foreground + " PID=" + foregroundProcess + " process=" + foregroundName + " class=" + foregroundClass;
            // Never inject an activation gesture into an occluding window or
            // any other process, even when Windows denies foreground requests.
            if (hitProcess != ownProcess || GetAncestor(hit, 2) != hwnd) return false;
            if (!LeftButton(true)) return false;
            try { return true; }
            finally { LeftButton(false); }
        } finally { SetWindowPos(hwnd, (IntPtr)(-2), 0, 0, 0, 0, 0x0013); }
    }
    public static bool LeftButton(bool down) {
        var inputs = new[] { new Input { Type = 0, Mouse = new MouseInput { Flags = down ? 2u : 4u } } };
        return SendInput(1, inputs, Marshal.SizeOf(typeof(Input))) == 1;
    }
    public static bool Responsive(IntPtr hwnd) {
        if (hwnd == IntPtr.Zero || !IsWindow(hwnd)) return false;
        IntPtr result;
        return SendMessageTimeout(hwnd, 0, IntPtr.Zero, null, 2, 250, out result) != IntPtr.Zero;
    }
    public static bool SetText(IntPtr hwnd, string value) {
        if (hwnd == IntPtr.Zero || !IsWindow(hwnd)) return false;
        IntPtr result;
        return SendMessageTimeout(hwnd, 12, IntPtr.Zero, value, 2, 500, out result) != IntPtr.Zero;
    }
    public static bool Click(IntPtr control) {
        if (control == IntPtr.Zero || !IsWindow(control)) return false;
        return PostMessage(GetParent(control), 0x0111, (IntPtr)GetDlgCtrlID(control), control);
    }
    public static IntPtr Control(IntPtr parent, int id) {
        if (parent == IntPtr.Zero || !IsWindow(parent)) return IntPtr.Zero;
        IntPtr result = IntPtr.Zero;
        EnumChildWindows(parent, (hwnd, data) => {
            if (GetDlgCtrlID(hwnd) == id) { result = hwnd; return false; }
            return true;
        }, IntPtr.Zero);
        return result;
    }
    public static IntPtr ClosePrompt(uint processId, IntPtr mainWindow) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((hwnd, data) => {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner == processId && GetWindow(hwnd, 4) == mainWindow && GetDlgItem(hwnd, 6) != IntPtr.Zero) {
                result = hwnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
    public static string OwnedDialogText(uint processId, IntPtr mainWindow) {
        var message = new StringBuilder();
        EnumWindows((hwnd, data) => {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner == processId && GetWindow(hwnd, 4) == mainWindow) {
                EnumChildWindows(hwnd, (child, unused) => {
                    var className = new StringBuilder(64);
                    GetClassName(child, className, className.Capacity);
                    if (className.ToString() == "Static") { message.Append(Text(child)); message.Append(" "); }
                    return true;
                }, IntPtr.Zero);
            }
            return true;
        }, IntPtr.Zero);
        return message.ToString().Trim();
    }
    public static IntPtr MainWindow(uint processId) {
        IntPtr result = IntPtr.Zero;
        EnumWindows((hwnd, data) => {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner == processId && GetWindow(hwnd, 4) == IntPtr.Zero && GetDlgItem(hwnd, 1000) != IntPtr.Zero) {
                result = hwnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
'@
}
$previousDpiContext = [RecorderLifecycleTest]::SetThreadDpiAwarenessContext([IntPtr](-4))
$previousForeground = [RecorderLifecycleTest]::GetForegroundWindow()
$previousCursor = New-Object RecorderLifecycleTest+Point
[RecorderLifecycleTest]::GetCursorPos([ref]$previousCursor) | Out-Null
$script:leftButtonDown = $false

function Wait-ForCondition([scriptblock]$Condition, [string]$Failure) {
    $deadline = [DateTime]::UtcNow.AddSeconds(5)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 50
    }
    throw $Failure
}

function Start-TestRecorder {
    $recorderProcess = Start-Process -FilePath $Executable -PassThru -WindowStyle Hidden
    try {
        Wait-ForCondition {
            if ($recorderProcess.HasExited) { return $false }
            $readyWindow = [RecorderLifecycleTest]::MainWindow($recorderProcess.Id)
            $readyWindow -ne [IntPtr]::Zero -and
                [RecorderLifecycleTest]::Control($readyWindow, 1010) -ne [IntPtr]::Zero -and
                [RecorderLifecycleTest]::Control($readyWindow, 1016) -ne [IntPtr]::Zero -and
                [RecorderLifecycleTest]::Control($readyWindow, 1017) -ne [IntPtr]::Zero -and
                [RecorderLifecycleTest]::Control($readyWindow, 1018) -ne [IntPtr]::Zero -and
                [RecorderLifecycleTest]::IsWindowEnabled([RecorderLifecycleTest]::Control($readyWindow, 1016)) -and
                -not [RecorderLifecycleTest]::IsWindowEnabled([RecorderLifecycleTest]::Control($readyWindow, 1017)) -and
                [RecorderLifecycleTest]::Responsive($readyWindow)
        } 'Recorder window and recording controls did not initialize within five seconds.'
        return $recorderProcess
    } catch {
        if (-not $recorderProcess.HasExited) { $recorderProcess.Kill(); $recorderProcess.WaitForExit(5000) | Out-Null }
        $recorderProcess.Dispose()
        throw
    }
}

function Assert-NormalExit($RecorderProcess, [string]$Route, [int]$Command = 0) {
    if (-not $RecorderProcess.WaitForExit(5000)) { throw "$Route did not close the recorder within five seconds." }
    # MFC returns the final message's wParam on a normal dialog exit, so
    # IDOK/IDCANCEL and the save/discard choices can be the process exit code.
    if ($RecorderProcess.ExitCode -notin @(0, $Command)) {
        throw "$Route exited unexpectedly: exit code $($RecorderProcess.ExitCode)."
    }
}

function Stop-TestRecorder($RecorderProcess) {
    if ($script:leftButtonDown) {
        [RecorderLifecycleTest]::LeftButton($false) | Out-Null
        $script:leftButtonDown = $false
    }
    if (-not $RecorderProcess.HasExited) { $RecorderProcess.Kill(); $RecorderProcess.WaitForExit(5000) | Out-Null }
    $RecorderProcess.Dispose()
}

function Press-TestControl([IntPtr]$Window, [IntPtr]$Control) {
    if ([RecorderLifecycleTest]::GetForegroundWindow() -ne $Window) {
        throw 'The test recorder lost foreground focus; refusing to inject input into another application.'
    }
    if (-not [RecorderLifecycleTest]::MoveToControl($Control)) { throw 'Failed to position the cursor over the test control.' }
    if (-not [RecorderLifecycleTest]::LeftButton($true)) { throw 'Windows rejected the test mouse press.' }
    $script:leftButtonDown = $true
}

function Release-TestButton {
    if (-not [RecorderLifecycleTest]::LeftButton($false)) { throw 'Windows rejected the test mouse release.' }
    $script:leftButtonDown = $false
}

function Request-TestClose([IntPtr]$Window, [string]$StartMode) {
    if ($StartMode -eq 'Command') {
        if (-not [RecorderLifecycleTest]::PostMessage($Window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)) {
            throw 'Failed to request closing the test recorder.'
        }
    } else {
        Press-TestControl $Window ([RecorderLifecycleTest]::Control($Window, 1))
        Release-TestButton
    }
}

$closeRoutes = @(
    @{ Name = 'Title-bar close'; Message = 0x0010; Command = 0 },
    @{ Name = 'Close button'; Message = 0x0111; Command = 1 },
    @{ Name = 'Main dialog Cancel command'; Message = 0x0111; Command = 2 }
)

try {
    $historyDirectory = Split-Path -Parent $historyPath
    New-Item -ItemType Directory -Path $historyDirectory -Force | Out-Null
    [System.IO.File]::WriteAllText($historyPath, '[]')
    foreach ($route in $(if ($InputMode -ne 'Real') { $closeRoutes })) {
        $recorderProcess = Start-TestRecorder
        try {
            $window = [RecorderLifecycleTest]::MainWindow($recorderProcess.Id)
            if (-not [RecorderLifecycleTest]::PostMessage($window, $route.Message, [IntPtr]$route.Command, [IntPtr]::Zero)) {
                throw "Failed to deliver $($route.Name)."
            }
            Assert-NormalExit $recorderProcess $route.Name $route.Command
            Write-Host "[PASS] $($route.Name) closes normally."
        } finally { Stop-TestRecorder $recorderProcess }
    }

    $recordingCases = @()
    if ($InputMode -ne 'Real') {
        $recordingCases += @{ Action = 'Discard'; Start = 'Command' }, @{ Action = 'Save'; Start = 'Command' }
    }
    if ($InputMode -ne 'Command') {
        $recordingCases += @{ Action = 'Save'; Start = 'RealClick' }, @{ Action = 'Discard'; Start = 'HeldButton' }
    }
    foreach ($recordingCase in $recordingCases) {
        $action = $recordingCase.Action
        $startMode = $recordingCase.Start
        $recorderProcess = Start-TestRecorder
        try {
            $window = [RecorderLifecycleTest]::MainWindow($recorderProcess.Id)
            $archivePath = Join-Path $outputDirectory "$startMode-$action.zip"
            $pathControl = [RecorderLifecycleTest]::Control($window, 1010)
            $startControl = [RecorderLifecycleTest]::Control($window, 1016)
            $stopControl = [RecorderLifecycleTest]::Control($window, 1017)
            if (-not [RecorderLifecycleTest]::SetText($pathControl, $archivePath)) { throw 'Failed to set test ZIP destination.' }
            if ($startMode -eq 'Command') {
                if (-not [RecorderLifecycleTest]::Click($startControl)) { throw 'Failed to start test recording.' }
            } else {
                [RecorderLifecycleTest]::Show($window)
                if ([RecorderLifecycleTest]::GetForegroundWindow() -ne $window) {
                    try {
                        Wait-ForCondition {
                            [RecorderLifecycleTest]::GetForegroundWindow() -eq $window -or [RecorderLifecycleTest]::ActivateByOwnClick($window)
                        } 'Could not activate the test recorder through an ownership-checked mouse gesture.'
                    } catch {
                        throw "$($_.Exception.Message) $([RecorderLifecycleTest]::ActivationDetails)"
                    }
                }
                Wait-ForCondition {
                    [RecorderLifecycleTest]::GetForegroundWindow() -eq $window
                } 'The test recorder could not become the foreground window.'
                Press-TestControl $window $startControl
                if ($startMode -eq 'HeldButton') {
                    # Begin startup with a real left press still outstanding.
                    # The command prevents waiting for the button's release.
                    if (-not [RecorderLifecycleTest]::Click($startControl)) { throw 'Failed to start the held-button test.' }
                } else {
                    Start-Sleep -Milliseconds 100
                    Release-TestButton
                }
            }
            try {
                Wait-ForCondition {
                    [RecorderLifecycleTest]::IsWindowEnabled($stopControl) -and [RecorderLifecycleTest]::Responsive($window)
                } "Start Recording hung or did not enter the recording state within five seconds ($startMode)."
            } catch {
                $responsive = [RecorderLifecycleTest]::Responsive($window)
                $stopEnabled = [RecorderLifecycleTest]::IsWindowEnabled($stopControl)
                $dialogText = [RecorderLifecycleTest]::OwnedDialogText($recorderProcess.Id, $window)
                throw "$($_.Exception.Message) UI responsive=$responsive; Stop enabled=$stopEnabled. Dialog: $dialogText"
            }
            if ($startMode -eq 'HeldButton') { Release-TestButton }
            Write-Host "[PASS] Start Recording remains responsive ($startMode/$action case)."

            if ($startMode -ne 'Command') {
                # Target a passive recorder-owned label, so this verifies the
                # actual OS hook and screenshot pipeline without other apps.
                $countControl = [RecorderLifecycleTest]::Control($window, 1018)
                Press-TestControl $window $countControl
                Release-TestButton
                Wait-ForCondition {
                    $countText = [RecorderLifecycleTest]::Text($countControl)
                    $countText -match 'Steps recorded: ([1-9][0-9]*)' -and [RecorderLifecycleTest]::Responsive($window)
                } "Real mouse input did not produce a step and responsive UI within five seconds ($startMode)."
                Write-Host "[PASS] Real mouse input records a step without hanging ($startMode)."

                if ($startMode -eq 'RealClick') {
                    # Reuse the same dialog to expose stale hook-thread or
                    # callback state left behind by a previous session.
                    Press-TestControl $window $stopControl
                    Release-TestButton
                    Wait-ForCondition {
                        [RecorderLifecycleTest]::ClosePrompt($recorderProcess.Id, $window) -ne [IntPtr]::Zero
                    } 'Stopping the first real-input recording did not finish saving.'
                    $prompt = [RecorderLifecycleTest]::ClosePrompt($recorderProcess.Id, $window)
                    [RecorderLifecycleTest]::PostMessage($prompt, 0x0111, [IntPtr]7, [IntPtr]::Zero) | Out-Null
                    Wait-ForCondition {
                        -not [RecorderLifecycleTest]::IsWindow($prompt) -and [RecorderLifecycleTest]::IsWindowEnabled($startControl) -and [RecorderLifecycleTest]::Responsive($window)
                    } 'The recorder did not become ready for another session.'
                    $archivePath = Join-Path $outputDirectory 'RealClick-Restart-Save.zip'
                    if (-not [RecorderLifecycleTest]::SetText($pathControl, $archivePath)) { throw 'Failed to set the restarted recording destination.' }
                    Press-TestControl $window $startControl
                    Release-TestButton
                    Wait-ForCondition {
                        [RecorderLifecycleTest]::IsWindowEnabled($stopControl) -and [RecorderLifecycleTest]::Responsive($window)
                    } 'Restarting with a real Start click hung the recorder.'
                    Press-TestControl $window $countControl
                    Release-TestButton
                    Wait-ForCondition {
                        [RecorderLifecycleTest]::Text($countControl) -match 'Steps recorded: ([1-9][0-9]*)' -and [RecorderLifecycleTest]::Responsive($window)
                    } 'The restarted recording did not capture real input while remaining responsive.'
                    Write-Host '[PASS] A second real-input session in the same window records normally.'
                }
            }

            Request-TestClose $window $startMode
            Wait-ForCondition {
                [RecorderLifecycleTest]::ClosePrompt($recorderProcess.Id, $window) -ne [IntPtr]::Zero
            } 'Active recording did not offer the save/discard/cancel prompt.'
            $prompt = [RecorderLifecycleTest]::ClosePrompt($recorderProcess.Id, $window)
            [RecorderLifecycleTest]::PostMessage($prompt, 0x0111, [IntPtr]2, [IntPtr]::Zero) | Out-Null
            Wait-ForCondition {
                -not [RecorderLifecycleTest]::IsWindow($prompt) -and [RecorderLifecycleTest]::IsWindowEnabled($stopControl) -and [RecorderLifecycleTest]::Responsive($window)
            } 'Cancel did not leave the recording active and responsive.'
            Write-Host '[PASS] Cancel closing keeps the recording active.'

            Request-TestClose $window $startMode
            Wait-ForCondition {
                [RecorderLifecycleTest]::ClosePrompt($recorderProcess.Id, $window) -ne [IntPtr]::Zero
            } 'Closing again did not show the recording prompt.'
            $prompt = [RecorderLifecycleTest]::ClosePrompt($recorderProcess.Id, $window)
            $choice = if ($action -eq 'Save') { 6 } else { 7 }
            [RecorderLifecycleTest]::PostMessage($prompt, 0x0111, [IntPtr]$choice, [IntPtr]::Zero) | Out-Null
            Assert-NormalExit $recorderProcess "Active close/$action" $choice

            if ($action -eq 'Save') {
                Add-Type -AssemblyName System.IO.Compression.FileSystem
                $archive = [System.IO.Compression.ZipFile]::OpenRead($archivePath)
                try {
                    if ($null -eq $archive.GetEntry('steps.html') -or $null -eq $archive.GetEntry('steps.json')) {
                        throw 'Saved ZIP is missing the recording report or metadata.'
                    }
                    if ($startMode -ne 'Command') {
                        $reader = [System.IO.StreamReader]::new($archive.GetEntry('steps.json').Open())
                        try { $savedSteps = ConvertFrom-Json -InputObject $reader.ReadToEnd() } finally { $reader.Dispose() }
                        if (@($savedSteps).Count -lt 1) { throw 'The real-input archive contains no steps.' }
                        foreach ($step in $savedSteps) {
                            if ([string]::IsNullOrEmpty($step.image) -or $null -eq $archive.GetEntry($step.image)) {
                                throw 'A real-input step is missing its screenshot from the saved archive.'
                            }
                        }
                    }
                    foreach ($entry in $archive.Entries) {
                        $stream = $entry.Open()
                        try { $stream.CopyTo([System.IO.Stream]::Null) } finally { $stream.Dispose() }
                    }
                } finally { $archive.Dispose() }
            } elseif (Test-Path -LiteralPath $archivePath) {
                throw 'Discard unexpectedly saved an archive.'
            }
            Write-Host "[PASS] Active close/$startMode/$action exits normally with the expected archive behavior."
        } finally { Stop-TestRecorder $recorderProcess }
    }
} finally {
    if ($script:leftButtonDown) {
        [RecorderLifecycleTest]::LeftButton($false) | Out-Null
        $script:leftButtonDown = $false
    }
    [RecorderLifecycleTest]::SetCursorPos($previousCursor.X, $previousCursor.Y) | Out-Null
    if ([RecorderLifecycleTest]::IsWindow($previousForeground)) {
        [RecorderLifecycleTest]::Activate($previousForeground)
    }
    # Integration runs add test sessions to app history. Restore its exact bytes
    # only after all processes launched by this test have exited.
    if ($historyExisted) {
        [System.IO.File]::WriteAllBytes($historyPath, $historyBytes)
    } elseif (Test-Path -LiteralPath $historyPath) {
        Remove-Item -LiteralPath $historyPath
    }
    if ($previousDpiContext -ne [IntPtr]::Zero) {
        [RecorderLifecycleTest]::SetThreadDpiAwarenessContext($previousDpiContext) | Out-Null
    }
}
