#pragma once

#include "native/Types.h"
#include <windows.h>
#include <functional>
#include <memory>
#include <string>

namespace steprec::core {

enum class InputEventType {
    Click,
    Key,
    Type
};

struct InputEvent {
    InputEventType type{ InputEventType::Click };
    std::wstring kind;          // "click", "key", "type"
    std::wstring description;
    HWND hwnd{ nullptr };
    POINT point{ 0, 0 };
    bool isTypingContinuation{ false };
    wchar_t typedChar{ 0 };
    int delayMs{ 400 };
};

class InputHook {
public:
    using EventCallback = std::function<void(const InputEvent& event)>;

    static InputHook& Instance();

    bool Install(EventCallback callback, const CaptureOptions& options);
    void Uninstall(bool flushPendingEvents = false);
    bool IsInstalled() const;
    bool IsMouseHookInstalled() const;
    bool IsKeyboardHookInstalled() const;
    DWORD GetHookThreadId() const;
    const std::wstring& GetLastErrorMessage() const { return m_lastError; }
    void ProcessPendingEvents();
    void UpdateOptions(const CaptureOptions& options);

    static wchar_t DecodeVkChar(UINT vk, UINT scan);
    static std::wstring GetKeyName(const KBDLLHOOKSTRUCT& k);

private:
    InputHook() = default;
    ~InputHook() { Uninstall(); }

    struct WorkerState;
    static DWORD WINAPI HookThreadProc(void* context);
    static thread_local WorkerState* s_threadState;
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    std::shared_ptr<WorkerState> m_worker;
    EventCallback m_callback;
    CaptureOptions m_options;
    std::wstring m_lastError;

    DWORD m_typingLastTick{ 0 };
    std::wstring m_currentTypingText;
    HWND m_lastTypingHwnd{ nullptr };
};

} // namespace steprec::core
