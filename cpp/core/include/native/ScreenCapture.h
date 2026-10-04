#pragma once

#include <windows.h>
#include <string>

namespace steprec::core {

class ScopedGdiplus {
public:
    ScopedGdiplus();
    ~ScopedGdiplus();

    ScopedGdiplus(const ScopedGdiplus&) = delete;
    ScopedGdiplus& operator=(const ScopedGdiplus&) = delete;

    bool IsInitialized() const { return m_token != 0; }

private:
    ULONG_PTR m_token{ 0 };
};

class ScreenCapture {
public:
    static std::wstring GetWindowTitle(HWND hwnd);
    static std::wstring GetProcessName(HWND hwnd);
    static HWND GetWindowAt(POINT pt);
    static bool GetWindowBounds(HWND hwnd, RECT& outRect);
    static POINT GetCurrentCursorPos();

    static bool CaptureArea(int left, int top, int width, int height,
                           const std::wstring& outputPath,
                           POINT clickMarker, bool drawMarker);

    static bool ExecuteShot(HWND targetHwnd, long clickX, long clickY,
                           bool fullScreen, bool highlight,
                           const std::wstring& outputPath);
};

} // namespace steprec::core
