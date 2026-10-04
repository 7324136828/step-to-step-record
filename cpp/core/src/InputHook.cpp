#include "native/InputHook.h"
#include "native/ScreenCapture.h"
#include <array>
#include <atomic>
#include <deque>
#include <mutex>

namespace steprec::core {

namespace {
constexpr DWORD kHookWaitMs = 2000;

struct RawInput {
    bool mouse{ false };
    WPARAM message{ 0 };
    KBDLLHOOKSTRUCT keyData{};
    POINT point{};
    HWND foreground{ nullptr };
    std::array<BYTE, 256> keyboardState{};
};

std::wstring WindowsError(const wchar_t* operation, DWORD error) {
    wchar_t* text = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, error, 0, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
    std::wstring message = operation;
    message += L" (Windows error " + std::to_wstring(error) + L")";
    if (text) {
        message += L": ";
        message += text;
        LocalFree(text);
    }
    return message;
}
} // namespace

struct InputHook::WorkerState {
    HANDLE ready{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
    HANDLE stop{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
    HANDLE thread{ nullptr };
    DWORD threadId{ 0 };
    HHOOK mouseHook{ nullptr };
    HHOOK keyHook{ nullptr };
    std::atomic<bool> accepting{ false };
    std::atomic<bool> mouseEnabled{ false };
    std::atomic<bool> keysEnabled{ false };
    std::atomic<bool> mouseInstalled{ false };
    std::atomic<bool> keysInstalled{ false };
    DWORD installError{ ERROR_SUCCESS };
    std::array<BYTE, 256> keyboardState{};
    std::mutex queueMutex;
    std::deque<RawInput> pending;

    ~WorkerState() {
        if (thread) CloseHandle(thread);
        if (ready) CloseHandle(ready);
        if (stop) CloseHandle(stop);
    }

    void Queue(RawInput input) noexcept {
        if (!accepting.load()) return;
        try {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (accepting.load()) pending.push_back(std::move(input));
        } catch (...) {
            // Never unwind a C++ allocation/locking failure through User32's
            // low-level hook callback. Leave the input available to its target.
        }
    }
};

thread_local InputHook::WorkerState* InputHook::s_threadState = nullptr;

InputHook& InputHook::Instance() {
    static InputHook instance;
    return instance;
}

bool InputHook::Install(EventCallback callback, const CaptureOptions& options) {
    Uninstall();
    m_lastError.clear();
    if (!options.recordClicks && !options.recordKeys) {
        m_lastError = L"Enable mouse clicks or keyboard input before recording.";
        return false;
    }
    m_callback = std::move(callback);
    m_options = options;
    m_typingLastTick = 0;
    m_currentTypingText.clear();
    m_lastTypingHwnd = nullptr;

    auto state = std::make_shared<WorkerState>();
    if (!state->ready || !state->stop) {
        m_lastError = WindowsError(L"Could not create input hook events", GetLastError());
        m_callback = {};
        return false;
    }
    state->mouseEnabled = options.recordClicks;
    state->keysEnabled = options.recordKeys;
    // The worker owns its context independently. A startup timeout must not
    // leave it pointing at a recorder or wait indefinitely for a stalled API.
    auto* context = new std::shared_ptr<WorkerState>(state);
    state->thread = CreateThread(nullptr, 0, HookThreadProc, context, 0, &state->threadId);
    if (!state->thread) {
        delete context;
        m_lastError = WindowsError(L"Could not create input hook thread", GetLastError());
        m_callback = {};
        return false;
    }
    m_worker = state;
    const DWORD wait = WaitForSingleObject(state->ready, kHookWaitMs);
    if (wait != WAIT_OBJECT_0) {
        m_lastError = wait == WAIT_TIMEOUT
            ? L"Windows input hook setup did not complete within two seconds. Please try recording again."
            : WindowsError(L"Could not wait for input hook setup", GetLastError());
        state->accepting = false;
        SetEvent(state->stop);
        m_worker.reset();
        m_callback = {};
        return false;
    }
    if (state->installError != ERROR_SUCCESS || !IsInstalled()) {
        m_lastError = WindowsError(L"Could not install all requested input hooks",
            state->installError ? state->installError : ERROR_GEN_FAILURE);
        Uninstall();
        return false;
    }
    return true;
}

DWORD WINAPI InputHook::HookThreadProc(void* context) {
    std::unique_ptr<std::shared_ptr<WorkerState>> holder(
        static_cast<std::shared_ptr<WorkerState>*>(context));
    auto state = *holder;
    holder.reset();
    s_threadState = state.get();
    MSG message{};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    for (int vk = 0; vk < 256; ++vk) {
        state->keyboardState[vk] = (GetAsyncKeyState(vk) & 0x8000) ? 0x80 : 0;
    }
    for (int vk : { VK_CAPITAL, VK_NUMLOCK, VK_SCROLL }) {
        state->keyboardState[vk] |= static_cast<BYTE>(GetKeyState(vk) & 1);
    }
    HMODULE module = GetModuleHandleW(nullptr);
    if (state->mouseEnabled) {
        state->mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, module, 0);
        if (!state->mouseHook) state->installError = GetLastError();
    }
    if (state->keysEnabled) {
        state->keyHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, module, 0);
        if (!state->keyHook && !state->installError) state->installError = GetLastError();
    }
    state->mouseInstalled = state->mouseHook != nullptr;
    state->keysInstalled = state->keyHook != nullptr;
    const bool installed = (!state->mouseEnabled || state->mouseInstalled) &&
                           (!state->keysEnabled || state->keysInstalled);
    state->accepting = installed && WaitForSingleObject(state->stop, 0) != WAIT_OBJECT_0;
    SetEvent(state->ready);

    while (installed && WaitForSingleObject(state->stop, 0) != WAIT_OBJECT_0) {
        const DWORD result = MsgWaitForMultipleObjects(1, &state->stop, FALSE, INFINITE, QS_ALLINPUT);
        if (result != WAIT_OBJECT_0 + 1) break;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                SetEvent(state->stop);
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    state->accepting = false;
    if (state->mouseHook) UnhookWindowsHookEx(state->mouseHook);
    if (state->keyHook) UnhookWindowsHookEx(state->keyHook);
    state->mouseInstalled = false;
    state->keysInstalled = false;
    s_threadState = nullptr;
    return 0;
}

void InputHook::Uninstall(bool flushPendingEvents) {
    auto state = m_worker;
    if (!state) return;
    state->accepting = false;
    SetEvent(state->stop);
    // The worker never calls the app or sends messages to its windows. Keep
    // even unexpected Windows teardown stalls bounded rather than joining forever.
    WaitForSingleObject(state->thread, kHookWaitMs);
    if (flushPendingEvents) ProcessPendingEvents();
    if (m_worker == state) {
        m_worker.reset();
        m_callback = {};
        m_currentTypingText.clear();
        m_lastTypingHwnd = nullptr;
    }
}

bool InputHook::IsInstalled() const {
    auto state = m_worker;
    return state && state->accepting &&
        (!m_options.recordClicks || state->mouseInstalled) &&
        (!m_options.recordKeys || state->keysInstalled);
}

bool InputHook::IsMouseHookInstalled() const {
    return m_worker && m_worker->mouseInstalled;
}

bool InputHook::IsKeyboardHookInstalled() const {
    return m_worker && m_worker->keysInstalled;
}

DWORD InputHook::GetHookThreadId() const {
    return m_worker ? m_worker->threadId : 0;
}

void InputHook::UpdateOptions(const CaptureOptions& options) {
    m_options = options;
    if (m_worker) {
        m_worker->mouseEnabled = options.recordClicks;
        m_worker->keysEnabled = options.recordKeys;
    }
}

wchar_t InputHook::DecodeVkChar(UINT vk, UINT scan) {
    BYTE state[256]{};
    if (!GetKeyboardState(state)) return 0;
    wchar_t buffer[8]{};
    return ToUnicodeEx(vk, scan, state, buffer, 8, 4, GetKeyboardLayout(0)) == 1 && buffer[0] >= L' '
        ? buffer[0] : 0;
}

std::wstring InputHook::GetKeyName(const KBDLLHOOKSTRUCT& key) {
    LONG parameter = static_cast<LONG>(key.scanCode << 16);
    if (key.flags & LLKHF_EXTENDED) parameter |= (1 << 24);
    wchar_t buffer[64]{};
    if (GetKeyNameTextW(parameter, buffer, 64) > 0) return buffer;
    return L"VK " + std::to_wstring(key.vkCode);
}

void InputHook::ProcessPendingEvents() {
    auto state = m_worker;
    if (!state) return;
    std::deque<RawInput> pending;
    {
        std::lock_guard<std::mutex> lock(state->queueMutex);
        pending.swap(state->pending);
    }
    for (const auto& raw : pending) {
        // Uninstall/restart from a callback invalidates the rest of this batch.
        if (m_worker != state || !m_callback) break;
        const auto options = m_options;
        InputEvent event;
        event.point = raw.point;
        if (raw.mouse) {
            if (!options.recordClicks) continue;
            event.hwnd = ScreenCapture::GetWindowAt(raw.point);
            const wchar_t* button = raw.message == WM_LBUTTONDOWN ? L"Left" :
                raw.message == WM_RBUTTONDOWN ? L"Right" : L"Middle";
            auto title = ScreenCapture::GetWindowTitle(event.hwnd);
            if (title.empty()) title = L"Unknown window";
            event.kind = L"click";
            event.description = std::wstring(button) + L" click at (" + std::to_wstring(raw.point.x) +
                L", " + std::to_wstring(raw.point.y) + L") on \"" + title + L"\"";
            event.delayMs = options.clickDelayMs;
            m_currentTypingText.clear();
            m_lastTypingHwnd = nullptr;
        } else {
            if (!options.recordKeys) continue;
            event.hwnd = raw.foreground;
            event.delayMs = options.keyDelayMs;
            DWORD windowThread = GetWindowThreadProcessId(event.hwnd, nullptr);
            wchar_t buffer[8]{};
            const wchar_t ch = ToUnicodeEx(raw.keyData.vkCode, raw.keyData.scanCode,
                raw.keyboardState.data(), buffer, 8, 4, GetKeyboardLayout(windowThread)) == 1 && buffer[0] >= L' '
                ? buffer[0] : 0;
            if (ch) {
                event.isTypingContinuation = !m_currentTypingText.empty() &&
                    raw.keyData.time - m_typingLastTick < options.typingGapMs &&
                    event.hwnd == m_lastTypingHwnd;
                if (!event.isTypingContinuation) m_currentTypingText.clear();
                m_currentTypingText += ch;
                m_lastTypingHwnd = event.hwnd;
                event.type = InputEventType::Type;
                event.kind = L"type";
                event.typedChar = ch;
                event.description = L"Typed text: \"" + m_currentTypingText + L"\"";
            } else {
                auto title = ScreenCapture::GetWindowTitle(event.hwnd);
                if (title.empty()) title = L"Unknown window";
                m_currentTypingText.clear();
                m_lastTypingHwnd = nullptr;
                event.type = InputEventType::Key;
                event.kind = L"key";
                event.description = L"Pressed " + GetKeyName(raw.keyData) + L" on \"" + title + L"\"";
            }
            m_typingLastTick = raw.keyData.time;
        }
        auto callback = m_callback;
        callback(event);
    }
}

LRESULT CALLBACK InputHook::LowLevelMouseProc(int code, WPARAM message, LPARAM data) {
    auto* state = s_threadState;
    if (code == HC_ACTION && state && state->mouseEnabled &&
        (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)) {
        RawInput input;
        input.mouse = true;
        input.message = message;
        input.point = reinterpret_cast<const MSLLHOOKSTRUCT*>(data)->pt;
        state->Queue(std::move(input));
    }
    return CallNextHookEx(nullptr, code, message, data);
}

LRESULT CALLBACK InputHook::LowLevelKeyboardProc(int code, WPARAM message, LPARAM data) {
    auto* state = s_threadState;
    if (code == HC_ACTION && state) {
        const auto& key = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        if (key.vkCode < state->keyboardState.size()) {
            BYTE& value = state->keyboardState[key.vkCode];
            if (down && !(value & 0x80) && (key.vkCode == VK_CAPITAL || key.vkCode == VK_NUMLOCK || key.vkCode == VK_SCROLL)) value ^= 1;
            value = static_cast<BYTE>((value & 1) | (down ? 0x80 : 0));
        }
        state->keyboardState[VK_SHIFT] = state->keyboardState[VK_LSHIFT] | state->keyboardState[VK_RSHIFT];
        state->keyboardState[VK_CONTROL] = state->keyboardState[VK_LCONTROL] | state->keyboardState[VK_RCONTROL];
        state->keyboardState[VK_MENU] = state->keyboardState[VK_LMENU] | state->keyboardState[VK_RMENU];
        if (down && state->keysEnabled) {
            RawInput input;
            input.keyData = key;
            input.foreground = GetForegroundWindow();
            GetCursorPos(&input.point);
            input.keyboardState = state->keyboardState;
            state->Queue(std::move(input));
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}

} // namespace steprec::core
