#pragma once

#include "native/Types.h"
#include "native/InputHook.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>

namespace steprec::core {

class StepRecorderCore {
public:
    using StepCallback = std::function<void(const Step& step)>;
    using StatusCallback = std::function<void(const std::wstring& statusText)>;
    using StateCallback = std::function<void(JobStatus newState)>;

    StepRecorderCore();
    ~StepRecorderCore();

    void SetCallbacks(StepCallback onStep, StatusCallback onStatus, StateCallback onState);

    bool Start(const std::wstring& workspaceDir,
               const std::wstring& zipPath,
               const CaptureOptions& options,
               HWND timerHwnd);

    bool Stop(std::wstring& outErrorMessage);
    void Cancel();

    void HandleTimer(UINT_PTR timerId);
    void ProcessPendingShots();

    bool IsRecording() const { return m_state == JobStatus::Recording; }
    JobStatus GetState() const { return m_state; }
    const std::vector<Step>& GetSteps() const { return m_steps; }
    size_t GetStepCount() const { return m_steps.size(); }
    const std::wstring& GetZipPath() const { return m_zipPath; }
    const std::wstring& GetWorkspaceDir() const { return m_workspaceDir; }
    const std::wstring& GetLastErrorMessage() const { return m_lastError; }

    // Direct shot execution helper (used by timer or selftest)
    void ExecuteShotForRequest(const ShotRequest& req);
    void InjectInputEvent(const InputEvent& event);

private:
    void OnInputReceived(const InputEvent& event);
    int AddStep(const wchar_t* kind, const std::wstring& desc, HWND hwnd,
                const std::wstring& windowTitle, const std::wstring& processName);
    UINT_PTR ScheduleDelayedShot(int stepIndex, POINT pt, HWND hwnd);
    void NotifyStateAndStatus(JobStatus state, const std::wstring& status);

    std::wstring FormatCurrentTime();

    mutable std::mutex m_mutex;
    JobStatus m_state{ JobStatus::Idle };
    CaptureOptions m_options;
    std::wstring m_workspaceDir;
    std::wstring m_shotsDir;
    std::wstring m_zipPath;
    std::wstring m_lastError;
    HWND m_timerHwnd{ nullptr };

    std::vector<Step> m_steps;
    int m_activeTypingStep{ -1 };

    std::map<UINT_PTR, ShotRequest> m_pendingShots;
    UINT_PTR m_nextTimerId{ 1000 };

    StepCallback m_onStep;
    StatusCallback m_onStatus;
    StateCallback m_onState;
};

} // namespace steprec::core
