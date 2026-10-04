#pragma once

#include "native/Types.h"
#include "native/StepRecorderCore.h"
#include "TempWorkspace.h"
#include "HistoryStore.h"
#include <memory>
#include <string>
#include <chrono>

namespace steprec::services {

class JobManager {
public:
    using StepCallback = std::function<void(const core::Step& step)>;
    using StatusCallback = std::function<void(const std::wstring& statusText)>;
    using StateCallback = std::function<void(core::JobStatus newState)>;

    static JobManager& Instance();

    void Initialize(HWND notificationHwnd);
    void SetCallbacks(StepCallback onStep, StatusCallback onStatus, StateCallback onState);

    bool StartSession(const std::wstring& targetZipPath,
                     const core::CaptureOptions& options,
                     std::wstring& errorMessage);

    bool StopSession(std::wstring& errorMessage);
    void CancelSession();

    bool IsActive() const;
    core::JobStatus GetCurrentState() const;
    size_t GetStepCount() const;
    const std::vector<core::Step>& GetSteps() const;
    const std::wstring& GetActiveZipPath() const;
    int GetElapsedSeconds() const;

    void HandleTimer(UINT_PTR timerId);
    void InjectInputEvent(const core::InputEvent& event);

private:
    JobManager();
    ~JobManager() = default;

    HWND m_notificationHwnd{ nullptr };
    core::StepRecorderCore m_core;
    std::unique_ptr<platform::TempWorkspace> m_workspace;

    core::JobMetadata m_currentJob;
    std::chrono::steady_clock::time_point m_startTime;

    StepCallback m_onStep;
    StatusCallback m_onStatus;
    StateCallback m_onState;
};

} // namespace steprec::services
