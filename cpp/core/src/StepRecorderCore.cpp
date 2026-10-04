#include "native/StepRecorderCore.h"
#include "native/ScreenCapture.h"
#include "native/ReportGenerator.h"
#include "native/ZipWriter.h"
#include <algorithm>
#include <fstream>

namespace steprec::core {

namespace {
constexpr UINT_PTR kInputPollTimer = 999;
constexpr UINT kInputPollIntervalMs = 15;
}

StepRecorderCore::StepRecorderCore() = default;

StepRecorderCore::~StepRecorderCore() {
    if (m_state == JobStatus::Recording) {
        Cancel();
    }
}

void StepRecorderCore::SetCallbacks(StepCallback onStep, StatusCallback onStatus, StateCallback onState) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_onStep = std::move(onStep);
    m_onStatus = std::move(onStatus);
    m_onState = std::move(onState);
}

void StepRecorderCore::NotifyStateAndStatus(JobStatus state, const std::wstring& status) {
    StateCallback onState;
    StatusCallback onStatus;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        onState = m_onState;
        onStatus = m_onStatus;
    }
    // A callback may query or update the recorder. Never call it with the mutex held.
    if (onState) onState(state);
    if (onStatus) onStatus(status);
}

std::wstring StepRecorderCore::FormatCurrentTime() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32] = { 0 };
    swprintf_s(buf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    return buf;
}

bool StepRecorderCore::Start(const std::wstring& workspaceDir,
                            const std::wstring& zipPath,
                            const CaptureOptions& options,
                            HWND timerHwnd) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_lastError.clear();
    if (m_state == JobStatus::Recording || m_state == JobStatus::Finalizing) {
        m_lastError = L"A recording is already active or being saved.";
        return false;
    }

    if (!options.recordClicks && !options.recordKeys) {
        m_lastError = L"Please enable at least one capture option.";
        auto onStatus = m_onStatus;
        lock.unlock();
        if (onStatus) onStatus(L"Please enable at least one capture option.");
        return false;
    }

    if (zipPath.empty() || workspaceDir.empty()) {
        m_lastError = L"Invalid destination or workspace path.";
        auto onStatus = m_onStatus;
        lock.unlock();
        if (onStatus) onStatus(L"Invalid destination or workspace path.");
        return false;
    }

    m_workspaceDir = workspaceDir;
    m_shotsDir = m_workspaceDir + L"\\screenshots";
    m_zipPath = zipPath;
    m_options = options;
    m_timerHwnd = timerHwnd;

    CreateDirectoryW(m_workspaceDir.c_str(), nullptr);
    CreateDirectoryW(m_shotsDir.c_str(), nullptr);

    m_steps.clear();
    m_pendingShots.clear();
    m_activeTypingStep = -1;
    m_nextTimerId = 1000;

    m_state = JobStatus::Recording;
    lock.unlock();

    // Native hooks run on their own thread and enqueue raw input. This callback
    // runs only when the recording window drains the queue, outside hook context.
    bool hookOk = InputHook::Instance().Install(
        [this](const InputEvent& evt) {
            this->OnInputReceived(evt);
        },
        m_options
    );

    if (!hookOk) {
        {
            std::lock_guard<std::mutex> stateLock(m_mutex);
            m_state = JobStatus::Failed;
            m_lastError = InputHook::Instance().GetLastErrorMessage();
        }
        NotifyStateAndStatus(JobStatus::Failed, L"Failed to install system hooks.");
        return false;
    }

    if (m_timerHwnd && !SetTimer(m_timerHwnd, kInputPollTimer, kInputPollIntervalMs, nullptr)) {
        InputHook::Instance().Uninstall();
        {
            std::lock_guard<std::mutex> stateLock(m_mutex);
            m_state = JobStatus::Failed;
            m_lastError = L"Could not start the input processing timer.";
        }
        NotifyStateAndStatus(JobStatus::Failed, m_lastError);
        return false;
    }

    NotifyStateAndStatus(JobStatus::Recording,
        L"Recording started... " + std::to_wstring(GetStepCount()) + L" steps");
    return true;
}

void StepRecorderCore::InjectInputEvent(const InputEvent& event) {
    OnInputReceived(event);
}

void StepRecorderCore::OnInputReceived(const InputEvent& event) {
    // Window APIs can dispatch messages, including another input hook callback.
    // Resolve window metadata before taking the recorder lock.
    const std::wstring windowTitle = ScreenCapture::GetWindowTitle(event.hwnd);
    const std::wstring processName = ScreenCapture::GetProcessName(event.hwnd);
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_state != JobStatus::Recording) return;

    int idx;
    bool newStep = false;
    if (event.isTypingContinuation && m_activeTypingStep >= 0 &&
        m_activeTypingStep < static_cast<int>(m_steps.size())) {
        idx = m_activeTypingStep;
        Step& s = m_steps[m_activeTypingStep];
        s.text += event.typedChar;
        s.description = event.description;
    } else {
        idx = AddStep(event.kind.c_str(), event.description, event.hwnd, windowTitle, processName);
        newStep = true;
        if (event.type == InputEventType::Type) {
            m_steps[idx].text = std::wstring(1, event.typedChar);
            m_activeTypingStep = idx;
        } else {
            m_activeTypingStep = -1;
        }
    }

    const Step step = m_steps[idx];
    const UINT_PTR timerId = ScheduleDelayedShot(idx, event.point, step.hwnd);
    const HWND timerHwnd = m_timerHwnd;
    auto onStep = m_onStep;
    auto onStatus = m_onStatus;
    lock.unlock();

    if (timerHwnd) SetTimer(timerHwnd, timerId, static_cast<UINT>((std::max)(1, event.delayMs)), nullptr);
    if (onStep) onStep(step);
    if (newStep && onStatus) {
        onStatus(L"Recording... " + std::to_wstring(step.n) + (step.n == 1 ? L" step" : L" steps"));
    }
}

int StepRecorderCore::AddStep(const wchar_t* kind, const std::wstring& desc, HWND hwnd,
                              const std::wstring& windowTitle, const std::wstring& processName) {
    Step s;
    s.n = static_cast<int>(m_steps.size()) + 1;
    s.time = FormatCurrentTime();
    s.kind = kind;
    s.description = desc;
    s.window = windowTitle;
    s.process = processName;
    s.hwnd = hwnd;

    wchar_t imgRel[64] = { 0 };
    swprintf_s(imgRel, L"screenshots/step_%03d.png", s.n);
    s.image = imgRel;

    m_steps.push_back(s);
    return static_cast<int>(m_steps.size()) - 1;
}

UINT_PTR StepRecorderCore::ScheduleDelayedShot(int stepIndex, POINT pt, HWND hwnd) {
    UINT_PTR timerId = m_nextTimerId++;
    ShotRequest req{ stepIndex, pt.x, pt.y, hwnd };
    m_pendingShots[timerId] = req;

    return timerId;
}

void StepRecorderCore::HandleTimer(UINT_PTR timerId) {
    if (timerId == kInputPollTimer) {
        InputHook::Instance().ProcessPendingEvents();
        return;
    }
    ShotRequest req;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_pendingShots.find(timerId);
        if (it != m_pendingShots.end()) {
            req = it->second;
            m_pendingShots.erase(it);
            found = true;
        }
    }

    if (found) {
        if (m_timerHwnd) {
            KillTimer(m_timerHwnd, timerId);
        }
        ExecuteShotForRequest(req);
    }
}

void StepRecorderCore::ExecuteShotForRequest(const ShotRequest& req) {
    std::wstring shotPath;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (req.stepIndex < 0 || req.stepIndex >= static_cast<int>(m_steps.size())) return;
        shotPath = m_workspaceDir + L"\\" + m_steps[req.stepIndex].image;
    }

    bool success = ScreenCapture::ExecuteShot(
        req.hwnd, req.x, req.y,
        m_options.fullScreen,
        m_options.highlightClicks,
        shotPath
    );

    if (!success) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (req.stepIndex >= 0 && req.stepIndex < static_cast<int>(m_steps.size())) {
            m_steps[req.stepIndex].description += L" [screenshot failed]";
        }
    }
}

void StepRecorderCore::ProcessPendingShots() {
    std::vector<ShotRequest> remaining;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& kv : m_pendingShots) {
            if (m_timerHwnd) {
                KillTimer(m_timerHwnd, kv.first);
            }
            remaining.push_back(kv.second);
        }
        m_pendingShots.clear();
    }

    for (const auto& req : remaining) {
        ExecuteShotForRequest(req);
    }
}

bool StepRecorderCore::Stop(std::wstring& outErrorMessage) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_state != JobStatus::Recording) {
            outErrorMessage = L"Recorder is not in active recording state.";
            return false;
        }
    }

    // Stop the producer before draining its final inputs while the core can
    // still accept them. Null-window callers use direct input injection.
    InputHook::Instance().Uninstall(m_timerHwnd != nullptr);
    if (m_timerHwnd) KillTimer(m_timerHwnd, kInputPollTimer);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = JobStatus::Finalizing;
    }
    NotifyStateAndStatus(JobStatus::Finalizing, L"Saving ZIP archive...");
    ProcessPendingShots();

    std::vector<ZipEntry> zipFiles;

    // 1. Gather all screenshots from screenshots directory
    std::wstring searchPattern = m_shotsDir + L"\\*.png";
    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring fullPath = m_shotsDir + L"\\" + fd.cFileName;
            std::ifstream file(fullPath, std::ios::binary);
            if (file) {
                ZipEntry entry;
                entry.name = "screenshots/" + Utf8Encode(fd.cFileName);
                entry.data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
                zipFiles.push_back(std::move(entry));
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    // Sort screenshots alphabetically
    std::sort(zipFiles.begin(), zipFiles.end(), [](const ZipEntry& a, const ZipEntry& b) {
        return a.name < b.name;
    });

    // 2. Build and add steps.html
    std::vector<Step> stepsCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        stepsCopy = m_steps;
    }

    std::wstring htmlContent = ReportGenerator::BuildHtml(stepsCopy);
    std::string htmlUtf8 = Utf8Encode(htmlContent);
    zipFiles.push_back({ "steps.html", std::vector<uint8_t>(htmlUtf8.begin(), htmlUtf8.end()) });

    // 3. Build and add steps.json
    std::string jsonContent = ReportGenerator::BuildJson(stepsCopy);
    zipFiles.push_back({ "steps.json", std::vector<uint8_t>(jsonContent.begin(), jsonContent.end()) });

    // 4. Write ZIP archive
    bool zipOk = ZipWriter::CreateZip(m_zipPath, zipFiles, outErrorMessage);
    if (!zipOk) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = JobStatus::Failed;
        }
        NotifyStateAndStatus(JobStatus::Failed, L"Failed to write ZIP archive.");
        return false;
    }

    // 5. Validate written ZIP
    size_t writtenCount = 0;
    std::wstring valErr;
    if (!ZipWriter::ValidateZip(m_zipPath, writtenCount, valErr)) {
        outErrorMessage = L"Archive validation failed: " + valErr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = JobStatus::Failed;
        }
        NotifyStateAndStatus(JobStatus::Failed, L"ZIP validation failed.");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = JobStatus::Completed;
    }
    NotifyStateAndStatus(JobStatus::Completed, L"Saved " + std::to_wstring(stepsCopy.size()) + L" steps to ZIP archive");

    return true;
}

void StepRecorderCore::Cancel() {
    InputHook::Instance().Uninstall();
    if (m_timerHwnd) KillTimer(m_timerHwnd, kInputPollTimer);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& kv : m_pendingShots) {
            if (m_timerHwnd) {
                KillTimer(m_timerHwnd, kv.first);
            }
        }
        m_pendingShots.clear();
        m_state = JobStatus::Discarded;
    }
    NotifyStateAndStatus(JobStatus::Discarded, L"Recording cancelled.");
}

} // namespace steprec::core
