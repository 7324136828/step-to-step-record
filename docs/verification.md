# Recorder startup and closing verification

Verified October 3, 2026, with MSVC v145 and the installed Windows SDK, Release x64.

The checkout's native source and build files were empty. Restored 68 empty files
from the project's local creation-session history, replaying its recorded source
patches. The surviving `JobManager.cpp` matched the recovered version exactly and
was preserved. Existing nonempty files were not overwritten during recovery.

The close handler called the default dialog close handler, which dispatched back
to the overridden cancel handler and recursively entered the close handler again.
The accepted close now directly ends the modal dialog through the base cancel
handler. Repeated close attempts are guarded, save failures are reported, and
pending notification payloads are released during shutdown.

The reported Start Recording freeze was reproduced with a real mouse press and
release. Its blocked UI stack was in USER32's `GetNextDlgTabItem`, reached through
`IsDialogMessage` before the Start command handler ran. The Work and History
dialogs had `DS_CONTROL`, but were children of a tab control without
`WS_EX_CONTROLPARENT`. Dialog focus traversal could not reach the focused page
control and looped. The pages are now direct children of the main dialog, and
`MapWindowPoints` converts the tab display rectangle into their parent's
coordinates. Microsoft's explanation of [DS_CONTROL and nested dialog navigation](https://devblogs.microsoft.com/oldnewthing/20040730-00/?p=38293)
describes why unreachable starting controls can make this traversal loop.

Before disabling Start, Stop, or recording options, the Work page conditionally
moves focus to an enabled control through the main dialog's `WM_NEXTDLGCTL`.
This prevents navigation from starting on a disabled control. The tab also uses
`WS_CLIPSIBLINGS` so its repaint does not cover the pages, which are now siblings.

The recovered recorder also held its mutex while installing hooks and invoking
external callbacks. This was a potential callback reentry deadlock, rather than
the proven cause of the reported mouse-click freeze. Hook installation, window
metadata lookup, timer creation, and external callbacks now run outside the
recorder mutex. Callback values and complete step notifications are copied before
invocation, and regression tests reenter `SetCallbacks` from notifications.

Native low-level input hooks now run on a dedicated thread with a message loop
and bounded startup/shutdown waits. Hook callbacks only queue native input
records. The UI timer drains the queue, resolves window metadata, decodes keys,
and invokes recorder callbacks on the application thread. Stop flushes queued
native input before finalizing the report and screenshots. Microsoft documents
the installing thread's message-loop requirement and recommends promptly handing
off work from dedicated low-level hooks: [LowLevelMouseProc documentation](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc).

Commands run successfully against the final build:

```powershell
./scripts/build.ps1 -Configuration Release -Platform x64
./scripts/test.ps1 -Configuration Release -Platform x64
```

Earlier validation passed the Release build, the earlier 14-case suite, the
application self-test, and command-driven dialog lifecycle checks. The self-test
produced three click/typing/Enter steps with their reports and screenshots. Those
checks bypassed real pointer focus routing and did not establish that mouse-click
startup worked. The recovered baseline and the build with only the hook-worker
change both failed the real-click startup check, prompting the blocked-stack
investigation and page-parent correction.

The final Release x64 build succeeded for the application, libraries, and tests.
The complete test script finished successfully with exit code 0:

- All 17 unit tests passed with no failures, including six recorder
  state/callback/input regressions and three dedicated hook startup,
  requested-hook coverage, replacement, and shutdown regressions.
- The application self-test recorded click, typed `hi`, and Enter as three steps
  and produced a valid 28,544-byte ZIP with reports and screenshots.
- Lifecycle mode `All` passed all three idle close routes and command-driven
  startup/close cancel, save, and discard cases.
- Native mouse press/release on Start and Stop passed. Real input produced steps
  and screenshots in the saved ZIP, a second recording in the same window
  remained responsive, and real closing supported cancel and save.
- Startup with the mouse button held also remained responsive, recorded a real
  input step, and supported real close cancel and discard without saving a ZIP.
- A separate native `SendInput` keyboard diagnostic passed with exit code 0:
  lowercase `h` continued to `hi`, Shift+A and Caps Lock+A produced uppercase
  `A`, and Ctrl+A produced key events without typed text. It restored the
  foreground window, pointer, Caps Lock state, and keyboard layout, and released
  synthetic held keys. This diagnostic is separate from the 17 unit tests and
  complete test script; its log is
  `build/verification/keyboard-diagnostic-result.txt`.

The lifecycle harness launches and controls its own recorder processes. Its real
input modes temporarily move the pointer and foreground its test window. The
complete runner snapshots and restores the original pointer and foreground window
around the entire suite, including the self-test, and restores the original
history bytes. Tests use bounded waits. The complete transcript is
`build/verification/Release/all-tests.log`, unit results are in
`build/verification/Release/unit-tests.stdout.log`, and the self-test summary is
`build/verification/Release/selftest_result.txt`. Lifecycle archives are in
separate `build/verification/lifecycle-*` directories, with the focused real-input
transcript at `build/verification/after-focus-fix/lifecycle-result.txt`.

The updated application is `build/bin/x64/Release/StepRecorder.exe`.
