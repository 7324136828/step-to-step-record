#include "JobManager.h"
#include <iomanip>
#include <sstream>

namespace steprec::services {

namespace {

std::wstring FormatTimestampNow() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64] = { 0 };
    swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::wstring GenerateJobId() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64] = { 0 };
    swprintf_s(buf, L"job_%04d%02d%02d_%02d%02d%02d_%u",
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, GetCurrentProcessId());
    return buf;
}

} // anonymous namespace

JobManager& JobManager::Instance() {
    static JobManager instance;
    return instance;
}

JobManager::JobManager() = default;

void JobManager::Initialize(HWND notificationHwnd) {
    m_notificationHwnd = notificationHwnd;

    m_core.SetCallbacks(
        [this](const core::Step& step) {
            if (m_onStep) m_onStep(step);
        },
        [this](const std::wstring& status) {
            if (m_onStatus) m_onStatus(status);
        },
        [this](core::JobStatus state) {
            if (m_onState) m_onState(state);
        }
    );
}

void JobManager::SetCallbacks(StepCallback onStep, StatusCallback onStatus, StateCallback onState) {
    m_onStep = std::move(onStep);
    m_onStatus = std::move(onStatus);
    m_onState = std::move(onState);
}

bool JobManager::StartSession(const std::wstring& targetZipPath,
                              const core::CaptureOptions& options,
                              std::wstring& errorMessage) {
    if (m_core.IsRecording()) {
        errorMessage = L"A recording session is already active.";
        return false;
    }

    m_workspace = std::make_unique<platform::TempWorkspace>(L"steprec");
    if (!m_workspace->Initialize(errorMessage)) {
        m_workspace.reset();
        return false;
    }

    m_startTime = std::chrono::steady_clock::now();
    m_currentJob = core::JobMetadata{};
    m_currentJob.id = GenerateJobId();
    m_currentJob.timestamp = FormatTimestampNow();
    m_currentJob.targetZipPath = targetZipPath;
    m_currentJob.status = L"Recording";

    HistoryStore::Instance().AddRecord(m_currentJob);

    bool started = m_core.Start(m_workspace->GetPath(), targetZipPath, options, m_notificationHwnd);
    if (!started) {
        errorMessage = m_core.GetLastErrorMessage();
        if (errorMessage.empty()) errorMessage = L"Failed to start recording session.";
        m_currentJob.status = L"Failed";
        m_currentJob.errorMessage = errorMessage;
        HistoryStore::Instance().UpdateRecord(m_currentJob);
        m_workspace->Cleanup();
        m_workspace.reset();
        return false;
    }

    return true;
}

bool JobManager::StopSession(std::wstring& errorMessage) {
    if (!m_core.IsRecording()) {
        errorMessage = L"No recording session is active.";
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count());

    m_currentJob.durationSeconds = elapsed;
    m_currentJob.stepCount = static_cast<int>(m_core.GetStepCount());
    m_currentJob.status = L"Saving ZIP...";
    HistoryStore::Instance().UpdateRecord(m_currentJob);

    bool ok = m_core.Stop(errorMessage);
    // Stop drains the hook thread's final input before writing the archive.
    m_currentJob.stepCount = static_cast<int>(m_core.GetStepCount());

    if (ok) {
        m_currentJob.status = L"Completed";
        m_currentJob.errorMessage.clear();
    } else {
        m_currentJob.status = L"Failed";
        m_currentJob.errorMessage = errorMessage;
    }

    HistoryStore::Instance().UpdateRecord(m_currentJob);

    if (m_workspace) {
        m_workspace->Cleanup();
        m_workspace.reset();
    }

    return ok;
}

void JobManager::CancelSession() {
    if (m_core.IsRecording()) {
        m_core.Cancel();
        m_currentJob.status = L"Discarded";
        m_currentJob.errorMessage = L"Recording discarded by user.";
        HistoryStore::Instance().UpdateRecord(m_currentJob);
    }
    if (m_workspace) {
        m_workspace->Cleanup();
        m_workspace.reset();
    }
}

bool JobManager::IsActive() const {
    return m_core.IsRecording();
}

core::JobStatus JobManager::GetCurrentState() const {
    return m_core.GetState();
}

size_t JobManager::GetStepCount() const {
    return m_core.GetStepCount();
}

const std::vector<core::Step>& JobManager::GetSteps() const {
    return m_core.GetSteps();
}

const std::wstring& JobManager::GetActiveZipPath() const {
    return m_core.GetZipPath();
}

int JobManager::GetElapsedSeconds() const {
    if (!m_core.IsRecording()) return m_currentJob.durationSeconds;
    auto now = std::chrono::steady_clock::now();
    return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count());
}

void JobManager::HandleTimer(UINT_PTR timerId) {
    m_core.HandleTimer(timerId);
}

void JobManager::InjectInputEvent(const core::InputEvent& event) {
    m_core.InjectInputEvent(event);
}

} // namespace steprec::services
