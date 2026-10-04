#include "TestHarness.h"
#include "native/ZipWriter.h"
#include "native/ReportGenerator.h"
#include "native/Types.h"
#include "native/StepRecorderCore.h"
#include "TempWorkspace.h"
#include <iostream>
#include <fstream>
#include <windows.h>

TEST_CASE(Core_Crc32StandardVector) {
    const char* data = "123456789";
    uint32_t crc = steprec::core::ZipWriter::CalculateCrc32(data, 9);
    // Standard CRC-32 test vector for "123456789" is 0xCBF43926
    ASSERT_EQ(crc, 0xCBF43926u);
    return true;
}

TEST_CASE(Core_ZipCreationAndValidation) {
    wchar_t tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring zipPath = std::wstring(tempDir) + L"core_test_archive.zip";
    DeleteFileW(zipPath.c_str());

    std::vector<steprec::core::ZipEntry> entries;
    std::string text1 = "Hello, world!";
    entries.push_back({ "test.txt", std::vector<uint8_t>(text1.begin(), text1.end()) });

    std::string text2 = "Subfolder content";
    entries.push_back({ "sub/folder/file.txt", std::vector<uint8_t>(text2.begin(), text2.end()) });

    std::wstring err;
    bool createOk = steprec::core::ZipWriter::CreateZip(zipPath, entries, err);
    ASSERT_TRUE(createOk);

    size_t count = 0;
    std::wstring valErr;
    bool valOk = steprec::core::ZipWriter::ValidateZip(zipPath, count, valErr);
    ASSERT_TRUE(valOk);
    ASSERT_EQ(count, 2u);

    DeleteFileW(zipPath.c_str());
    return true;
}

TEST_CASE(Core_ZipInvalidDestinationFolder) {
    std::wstring invalidPath = L"Z:\\non_existent_folder_12345\\test.zip";
    std::vector<steprec::core::ZipEntry> entries;
    entries.push_back({ "test.txt", { 'a', 'b', 'c' } });

    std::wstring err;
    bool ok = steprec::core::ZipWriter::CreateZip(invalidPath, entries, err);
    ASSERT_FALSE(ok);
    ASSERT_FALSE(err.empty());
    return true;
}

TEST_CASE(Core_ReportGeneratorHtmlAndJson) {
    std::vector<steprec::core::Step> steps;
    steprec::core::Step s1;
    s1.n = 1;
    s1.time = L"12:00:01";
    s1.kind = L"click";
    s1.description = L"Left click at (100, 200) on \"Test <App> & More\"";
    s1.window = L"Test <App>";
    s1.process = L"test.exe";
    s1.image = L"screenshots/step_001.png";
    steps.push_back(s1);

    steprec::core::Step s2;
    s2.n = 2;
    s2.time = L"12:00:05";
    s2.kind = L"type";
    s2.description = L"Typed text: \"hello world\"";
    s2.window = L"Notepad";
    s2.process = L"notepad.exe";
    s2.image = L"screenshots/step_002.png";
    steps.push_back(s2);

    std::wstring html = steprec::core::ReportGenerator::BuildHtml(steps, L"Custom Session Title");
    ASSERT_TRUE(html.find(L"Custom Session Title") != std::wstring::npos);
    ASSERT_TRUE(html.find(L"&lt;App&gt;") != std::wstring::npos); // Verify XML escaping
    ASSERT_TRUE(html.find(L"screenshots/step_001.png") != std::wstring::npos);
    ASSERT_TRUE(html.find(L"screenshots/step_002.png") != std::wstring::npos);

    std::string json = steprec::core::ReportGenerator::BuildJson(steps);
    ASSERT_TRUE(json.find("\"n\": 1") != std::string::npos);
    ASSERT_TRUE(json.find("\"n\": 2") != std::string::npos);
    ASSERT_TRUE(json.find("\"kind\": \"click\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"kind\": \"type\"") != std::string::npos);

    return true;
}

TEST_CASE(Core_StartCallbacksCanReplaceCallbacks) {
    steprec::platform::TempWorkspace workspace(L"core_start_callbacks");
    std::wstring error;
    ASSERT_TRUE(workspace.Initialize(error));

    std::vector<steprec::core::JobStatus> states;
    std::vector<std::wstring> statuses;
    bool stateMatchesNotification = true;
    steprec::core::StepRecorderCore::StatusCallback onStatus;
    steprec::core::StepRecorderCore::StateCallback onState;
    steprec::core::StepRecorderCore recorder;
    onStatus = [&](const std::wstring& text) {
        statuses.push_back(text);
        recorder.SetCallbacks({}, onStatus, onState);
    };
    onState = [&](steprec::core::JobStatus state) {
        states.push_back(state);
        stateMatchesNotification = stateMatchesNotification && recorder.GetState() == state;
        recorder.SetCallbacks({}, onStatus, onState);
    };
    recorder.SetCallbacks({}, onStatus, onState);

    const bool started = recorder.Start(workspace.GetPath(),
        workspace.GetPath() + L"\\session.zip", {}, nullptr);
    const bool wasRecording = recorder.IsRecording();
    const bool hooksWereInstalled = steprec::core::InputHook::Instance().IsInstalled();
    recorder.Cancel();
    recorder.SetCallbacks({}, {}, {});

    ASSERT_TRUE(started);
    ASSERT_TRUE(wasRecording);
    ASSERT_TRUE(hooksWereInstalled);
    ASSERT_TRUE(stateMatchesNotification);
    ASSERT_EQ(states.size(), 2u);
    ASSERT_EQ(states[0], steprec::core::JobStatus::Recording);
    ASSERT_EQ(states[1], steprec::core::JobStatus::Discarded);
    ASSERT_EQ(statuses.size(), 2u);
    ASSERT_EQ(statuses[0], L"Recording started... 0 steps");
    ASSERT_EQ(statuses[1], L"Recording cancelled.");
    ASSERT_FALSE(steprec::core::InputHook::Instance().IsInstalled());
    return true;
}

TEST_CASE(Core_TypingCallbacksReceiveCompleteStepsAndCanReplaceCallbacks) {
    steprec::platform::TempWorkspace workspace(L"core_typing_callbacks");
    std::wstring error;
    ASSERT_TRUE(workspace.Initialize(error));

    std::vector<steprec::core::Step> notifications;
    steprec::core::StepRecorderCore::StepCallback onStep;
    steprec::core::StepRecorderCore recorder;
    onStep = [&](const steprec::core::Step& step) {
        notifications.push_back(step);
        recorder.SetCallbacks(onStep, {}, {});
    };
    recorder.SetCallbacks(onStep, {}, {});
    ASSERT_TRUE(recorder.Start(workspace.GetPath(),
        workspace.GetPath() + L"\\session.zip", {}, nullptr));

    // Direct events avoid sending keyboard or mouse input to another application.
    // A null timer HWND leaves screenshots pending so Cancel can discard them.
    steprec::core::InputEvent typing;
    typing.type = steprec::core::InputEventType::Type;
    typing.kind = L"type";
    typing.typedChar = L'a';
    typing.description = L"Typed text: \"a\"";
    recorder.InjectInputEvent(typing);
    typing.isTypingContinuation = true;
    typing.typedChar = L'b';
    typing.description = L"Typed text: \"ab\"";
    recorder.InjectInputEvent(typing);

    steprec::core::InputEvent click;
    click.type = steprec::core::InputEventType::Click;
    click.kind = L"click";
    click.description = L"Left click at (10, 20)";
    click.point = { 10, 20 };
    recorder.InjectInputEvent(click);

    typing.isTypingContinuation = false;
    typing.typedChar = L'c';
    typing.description = L"Typed text: \"c\"";
    recorder.InjectInputEvent(typing);
    typing.isTypingContinuation = true;
    typing.typedChar = L'd';
    typing.description = L"Typed text: \"cd\"";
    recorder.InjectInputEvent(typing);

    const std::vector<steprec::core::Step> steps = recorder.GetSteps();
    recorder.Cancel();
    recorder.SetCallbacks({}, {}, {});

    ASSERT_EQ(notifications.size(), 5u);
    ASSERT_EQ(notifications[0].n, 1);
    ASSERT_EQ(notifications[0].text, L"a");
    ASSERT_EQ(notifications[0].description, L"Typed text: \"a\"");
    ASSERT_EQ(notifications[1].n, 1);
    ASSERT_EQ(notifications[1].text, L"ab");
    ASSERT_EQ(notifications[1].description, L"Typed text: \"ab\"");
    ASSERT_EQ(notifications[2].n, 2);
    ASSERT_EQ(notifications[2].kind, L"click");
    ASSERT_TRUE(notifications[2].text.empty());
    ASSERT_EQ(notifications[3].n, 3);
    ASSERT_EQ(notifications[3].text, L"c");
    ASSERT_EQ(notifications[4].n, 3);
    ASSERT_EQ(notifications[4].text, L"cd");
    ASSERT_EQ(steps.size(), 3u);
    ASSERT_EQ(steps[0].text, L"ab");
    ASSERT_EQ(steps[2].text, L"cd");
    ASSERT_EQ(steps[0].image, L"screenshots/step_001.png");
    ASSERT_EQ(steps[2].image, L"screenshots/step_003.png");
    ASSERT_EQ(GetFileAttributesW((workspace.GetPath() + L"\\" + steps[0].image).c_str()),
        INVALID_FILE_ATTRIBUTES);
    ASSERT_FALSE(steprec::core::InputHook::Instance().IsInstalled());
    return true;
}

TEST_CASE(Core_CancelCallbacksCanReplaceCallbacksAndRecordingCanRestart) {
    steprec::platform::TempWorkspace workspace(L"core_cancel_restart");
    std::wstring error;
    ASSERT_TRUE(workspace.Initialize(error));

    std::vector<steprec::core::JobStatus> states;
    size_t statusCount = 0;
    steprec::core::StepRecorderCore::StatusCallback onStatus;
    steprec::core::StepRecorderCore::StateCallback onState;
    steprec::core::StepRecorderCore recorder;
    onStatus = [&](const std::wstring&) {
        ++statusCount;
        recorder.SetCallbacks({}, onStatus, onState);
    };
    onState = [&](steprec::core::JobStatus state) {
        states.push_back(state);
        recorder.SetCallbacks({}, onStatus, onState);
    };
    recorder.SetCallbacks({}, onStatus, onState);
    ASSERT_TRUE(recorder.Start(workspace.GetPath(),
        workspace.GetPath() + L"\\session.zip", {}, nullptr));

    steprec::core::InputEvent event;
    event.kind = L"click";
    event.description = L"Test click";
    recorder.InjectInputEvent(event);
    recorder.Cancel();
    const bool cancelled = recorder.GetState() == steprec::core::JobStatus::Discarded;
    const bool hooksRemovedOnCancel = !steprec::core::InputHook::Instance().IsInstalled();
    recorder.InjectInputEvent(event);
    const size_t countAfterCancelledInput = recorder.GetStepCount();
    const bool restarted = recorder.Start(workspace.GetPath(),
        workspace.GetPath() + L"\\restart.zip", {}, nullptr);
    const size_t restartedStepCount = recorder.GetStepCount();
    const bool hooksReinstalled = steprec::core::InputHook::Instance().IsInstalled();
    recorder.Cancel();
    recorder.SetCallbacks({}, {}, {});

    ASSERT_TRUE(cancelled);
    ASSERT_TRUE(hooksRemovedOnCancel);
    ASSERT_EQ(countAfterCancelledInput, 1u);
    ASSERT_TRUE(restarted);
    ASSERT_EQ(restartedStepCount, 0u);
    ASSERT_TRUE(hooksReinstalled);
    ASSERT_EQ(states.size(), 4u);
    ASSERT_EQ(states[0], steprec::core::JobStatus::Recording);
    ASSERT_EQ(states[1], steprec::core::JobStatus::Discarded);
    ASSERT_EQ(states[2], steprec::core::JobStatus::Recording);
    ASSERT_EQ(states[3], steprec::core::JobStatus::Discarded);
    ASSERT_EQ(statusCount, 5u); // Two starts, one input, and two cancellations.
    ASSERT_FALSE(steprec::core::InputHook::Instance().IsInstalled());
    ASSERT_EQ(GetFileAttributesW((workspace.GetPath() + L"\\session.zip").c_str()),
        INVALID_FILE_ATTRIBUTES);
    return true;
}

TEST_CASE(Core_StopCallbacksCanReplaceCallbacksAndEmptyArchiveIsValid) {
    steprec::platform::TempWorkspace workspace(L"core_stop_callbacks");
    std::wstring error;
    ASSERT_TRUE(workspace.Initialize(error));
    const std::wstring zipPath = workspace.GetPath() + L"\\session.zip";

    std::vector<steprec::core::JobStatus> states;
    std::vector<std::wstring> statuses;
    steprec::core::StepRecorderCore::StatusCallback onStatus;
    steprec::core::StepRecorderCore::StateCallback onState;
    steprec::core::StepRecorderCore recorder;
    onStatus = [&](const std::wstring& text) {
        statuses.push_back(text);
        recorder.SetCallbacks({}, onStatus, onState);
    };
    onState = [&](steprec::core::JobStatus state) {
        states.push_back(state);
        recorder.SetCallbacks({}, onStatus, onState);
    };
    recorder.SetCallbacks({}, onStatus, onState);
    ASSERT_TRUE(recorder.Start(workspace.GetPath(), zipPath, {}, nullptr));
    const bool stopped = recorder.Stop(error);
    recorder.SetCallbacks({}, {}, {});

    ASSERT_TRUE(stopped);
    ASSERT_TRUE(error.empty());
    ASSERT_EQ(recorder.GetState(), steprec::core::JobStatus::Completed);
    ASSERT_FALSE(recorder.IsRecording());
    ASSERT_FALSE(steprec::core::InputHook::Instance().IsInstalled());
    ASSERT_EQ(states.size(), 3u);
    ASSERT_EQ(states[0], steprec::core::JobStatus::Recording);
    ASSERT_EQ(states[1], steprec::core::JobStatus::Finalizing);
    ASSERT_EQ(states[2], steprec::core::JobStatus::Completed);
    ASSERT_EQ(statuses.size(), 3u);
    ASSERT_EQ(statuses[1], L"Saving ZIP archive...");
    ASSERT_EQ(statuses[2], L"Saved 0 steps to ZIP archive");
    size_t entryCount = 0;
    ASSERT_TRUE(steprec::core::ZipWriter::ValidateZip(zipPath, entryCount, error));
    ASSERT_EQ(entryCount, 2u); // steps.html and steps.json, with no screenshots.
    ASSERT_FALSE(recorder.Stop(error));
    ASSERT_FALSE(error.empty());
    return true;
}

TEST_CASE(Core_FailedStopCallbacksCanReplaceCallbacksAndHooksAreRemoved) {
    steprec::platform::TempWorkspace workspace(L"core_failed_stop");
    std::wstring error;
    ASSERT_TRUE(workspace.Initialize(error));
    const std::wstring zipPath = workspace.GetPath() + L"\\session.zip";

    std::vector<steprec::core::JobStatus> states;
    std::vector<std::wstring> statuses;
    steprec::core::StepRecorderCore::StatusCallback onStatus;
    steprec::core::StepRecorderCore::StateCallback onState;
    steprec::core::StepRecorderCore recorder;
    onStatus = [&](const std::wstring& text) {
        statuses.push_back(text);
        recorder.SetCallbacks({}, onStatus, onState);
    };
    onState = [&](steprec::core::JobStatus state) {
        states.push_back(state);
        recorder.SetCallbacks({}, onStatus, onState);
    };
    recorder.SetCallbacks({}, onStatus, onState);
    ASSERT_TRUE(recorder.Start(workspace.GetPath(), zipPath, {}, nullptr));
    // Make only this test's destination unwritable without touching user files.
    const bool destinationBlocked = CreateDirectoryW(zipPath.c_str(), nullptr) != FALSE;
    const bool stopped = recorder.Stop(error);
    recorder.SetCallbacks({}, {}, {});

    ASSERT_TRUE(destinationBlocked);
    ASSERT_FALSE(stopped);
    ASSERT_FALSE(error.empty());
    ASSERT_EQ(recorder.GetState(), steprec::core::JobStatus::Failed);
    ASSERT_FALSE(steprec::core::InputHook::Instance().IsInstalled());
    ASSERT_EQ(states.size(), 3u);
    ASSERT_EQ(states[1], steprec::core::JobStatus::Finalizing);
    ASSERT_EQ(states[2], steprec::core::JobStatus::Failed);
    ASSERT_EQ(statuses.size(), 3u);
    ASSERT_EQ(statuses[2], L"Failed to write ZIP archive.");
    return true;
}

TEST_CASE(Core_InvalidStartCallsStatusOutsideLockAndNeverInstallsHooks) {
    steprec::platform::TempWorkspace workspace(L"core_invalid_start");
    std::wstring error;
    ASSERT_TRUE(workspace.Initialize(error));

    size_t stateCount = 0;
    std::vector<std::wstring> statuses;
    steprec::core::StepRecorderCore::StatusCallback onStatus;
    steprec::core::StepRecorderCore::StateCallback onState;
    steprec::core::StepRecorderCore recorder;
    onStatus = [&](const std::wstring& text) {
        statuses.push_back(text);
        recorder.SetCallbacks({}, onStatus, onState);
    };
    onState = [&](steprec::core::JobStatus) { ++stateCount; };
    recorder.SetCallbacks({}, onStatus, onState);

    steprec::core::CaptureOptions disabled;
    disabled.recordClicks = false;
    disabled.recordKeys = false;
    const std::wstring zipPath = workspace.GetPath() + L"\\session.zip";
    ASSERT_FALSE(recorder.Start(workspace.GetPath(), zipPath, disabled, nullptr));
    ASSERT_FALSE(steprec::core::InputHook::Instance().IsInstalled());
    ASSERT_FALSE(recorder.Start(L"", zipPath, {}, nullptr));
    ASSERT_FALSE(steprec::core::InputHook::Instance().IsInstalled());
    ASSERT_FALSE(recorder.Start(workspace.GetPath(), L"", {}, nullptr));
    ASSERT_FALSE(steprec::core::InputHook::Instance().IsInstalled());
    recorder.SetCallbacks({}, {}, {});

    ASSERT_EQ(recorder.GetState(), steprec::core::JobStatus::Idle);
    ASSERT_EQ(recorder.GetStepCount(), 0u);
    ASSERT_EQ(stateCount, 0u);
    ASSERT_EQ(statuses.size(), 3u);
    ASSERT_EQ(statuses[0], L"Please enable at least one capture option.");
    ASSERT_EQ(statuses[1], L"Invalid destination or workspace path.");
    ASSERT_EQ(statuses[2], L"Invalid destination or workspace path.");
    ASSERT_EQ(GetFileAttributesW(zipPath.c_str()), INVALID_FILE_ATTRIBUTES);
    return true;
}

TEST_CASE(Core_InputHookInstallsEveryEnabledHookOnDedicatedThread) {
    const DWORD callerThreadId = GetCurrentThreadId();
    steprec::core::CaptureOptions configurations[3];
    configurations[1].recordKeys = false;   // Mouse only.
    configurations[2].recordClicks = false; // Keyboard only.
    auto& hook = steprec::core::InputHook::Instance();

    for (const auto& options : configurations) {
        const bool installed = hook.Install([](const steprec::core::InputEvent&) {}, options);
        const bool reportedInstalled = hook.IsInstalled();
        const bool mouseInstalled = hook.IsMouseHookInstalled();
        const bool keyboardInstalled = hook.IsKeyboardHookInstalled();
        const DWORD workerThreadId = hook.GetHookThreadId();
        const std::wstring error = hook.GetLastErrorMessage();
        hook.Uninstall();

        if (!installed) {
            std::wcerr << L"Hook installation failed: " << error << std::endl;
        }
        ASSERT_TRUE(installed);
        ASSERT_TRUE(reportedInstalled);
        ASSERT_EQ(mouseInstalled, options.recordClicks);
        ASSERT_EQ(keyboardInstalled, options.recordKeys);
        ASSERT_NE(workerThreadId, 0u);
        ASSERT_NE(workerThreadId, callerThreadId);
        ASSERT_FALSE(hook.IsInstalled());
        ASSERT_FALSE(hook.IsMouseHookInstalled());
        ASSERT_FALSE(hook.IsKeyboardHookInstalled());
        ASSERT_EQ(hook.GetHookThreadId(), 0u);
    }
    return true;
}

TEST_CASE(Core_InputHookRejectsDisabledCaptureWithoutStartingWorker) {
    steprec::core::CaptureOptions options;
    options.recordClicks = false;
    options.recordKeys = false;
    auto& hook = steprec::core::InputHook::Instance();

    const bool installed = hook.Install([](const steprec::core::InputEvent&) {}, options);
    const bool reportedInstalled = hook.IsInstalled();
    const bool mouseInstalled = hook.IsMouseHookInstalled();
    const bool keyboardInstalled = hook.IsKeyboardHookInstalled();
    const DWORD workerThreadId = hook.GetHookThreadId();
    const std::wstring error = hook.GetLastErrorMessage();
    hook.Uninstall();
    hook.ProcessPendingEvents();

    ASSERT_FALSE(installed);
    ASSERT_FALSE(reportedInstalled);
    ASSERT_FALSE(mouseInstalled);
    ASSERT_FALSE(keyboardInstalled);
    ASSERT_EQ(workerThreadId, 0u);
    ASSERT_FALSE(error.empty());
    ASSERT_FALSE(hook.IsInstalled());
    ASSERT_EQ(hook.GetHookThreadId(), 0u);
    return true;
}

TEST_CASE(Core_InputHookCanReplaceAndRestartWorkerWithoutLeakingHooks) {
    const DWORD callerThreadId = GetCurrentThreadId();
    auto& hook = steprec::core::InputHook::Instance();
    steprec::core::CaptureOptions both;
    steprec::core::CaptureOptions mouseOnly;
    mouseOnly.recordKeys = false;

    for (int cycle = 0; cycle < 4; ++cycle) {
        const bool initialInstalled = hook.Install([](const steprec::core::InputEvent&) {}, both);
        const bool initialMouseInstalled = hook.IsMouseHookInstalled();
        const bool initialKeyboardInstalled = hook.IsKeyboardHookInstalled();
        const DWORD initialThreadId = hook.GetHookThreadId();

        // Replacement must stop the first worker before starting the requested hook set.
        const bool replacementInstalled = hook.Install(
            [](const steprec::core::InputEvent&) {}, mouseOnly);
        const bool replacementMouseInstalled = hook.IsMouseHookInstalled();
        const bool replacementKeyboardInstalled = hook.IsKeyboardHookInstalled();
        const DWORD replacementThreadId = hook.GetHookThreadId();
        hook.Uninstall();
        hook.Uninstall(); // Repeated shutdown is safe, including after replacement.
        hook.ProcessPendingEvents();

        ASSERT_TRUE(initialInstalled);
        ASSERT_TRUE(initialMouseInstalled);
        ASSERT_TRUE(initialKeyboardInstalled);
        ASSERT_NE(initialThreadId, 0u);
        ASSERT_NE(initialThreadId, callerThreadId);
        ASSERT_TRUE(replacementInstalled);
        ASSERT_TRUE(replacementMouseInstalled);
        ASSERT_FALSE(replacementKeyboardInstalled);
        ASSERT_NE(replacementThreadId, 0u);
        ASSERT_NE(replacementThreadId, callerThreadId);
        ASSERT_FALSE(hook.IsInstalled());
        ASSERT_FALSE(hook.IsMouseHookInstalled());
        ASSERT_FALSE(hook.IsKeyboardHookInstalled());
        ASSERT_EQ(hook.GetHookThreadId(), 0u);
    }
    return true;
}
