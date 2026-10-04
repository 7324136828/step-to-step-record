#include "native/ScreenCapture.h"
#include <dwmapi.h>
#include <gdiplus.h>
#include <vector>
#include <climits>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")

using namespace Gdiplus;

namespace steprec::core {

namespace {

bool GetPngEncoderClsid(CLSID& clsid) {
    UINT count = 0;
    UINT bytes = 0;
    GetImageEncodersSize(&count, &bytes);
    if (bytes == 0) return false;

    std::vector<BYTE> buf(bytes);
    auto* info = reinterpret_cast<ImageCodecInfo*>(buf.data());
    GetImageEncoders(count, bytes, info);
    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(info[i].MimeType, L"image/png") == 0) {
            clsid = info[i].Clsid;
            return true;
        }
    }
    return false;
}

} // anonymous namespace

ScopedGdiplus::ScopedGdiplus() {
    GdiplusStartupInput gsi;
    GdiplusStartup(&m_token, &gsi, nullptr);
}

ScopedGdiplus::~ScopedGdiplus() {
    if (m_token != 0) {
        GdiplusShutdown(m_token);
        m_token = 0;
    }
}

std::wstring ScreenCapture::GetWindowTitle(HWND hwnd) {
    if (!hwnd) return L"";
    wchar_t buf[512] = { 0 };
    int len = GetWindowTextW(hwnd, buf, 512);
    return len > 0 ? std::wstring(buf, len) : L"";
}

std::wstring ScreenCapture::GetProcessName(HWND hwnd) {
    if (!hwnd) return L"";
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return L"";

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return L"";

    wchar_t buf[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;
    std::wstring name;
    if (QueryFullProcessImageNameW(hProcess, 0, buf, &size)) {
        name = buf;
        size_t pos = name.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            name = name.substr(pos + 1);
        }
    }
    CloseHandle(hProcess);
    return name;
}

HWND ScreenCapture::GetWindowAt(POINT pt) {
    HWND hwnd = WindowFromPoint(pt);
    if (hwnd) {
        HWND root = GetAncestor(hwnd, GA_ROOT);
        if (root) hwnd = root;
    }
    return hwnd ? hwnd : GetForegroundWindow();
}

bool ScreenCapture::GetWindowBounds(HWND hwnd, RECT& outRect) {
    if (!hwnd) return false;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &outRect, sizeof(outRect)))) {
        return true;
    }
    return GetWindowRect(hwnd, &outRect) != FALSE;
}

POINT ScreenCapture::GetCurrentCursorPos() {
    POINT pt{};
    GetCursorPos(&pt);
    return pt;
}

bool ScreenCapture::CaptureArea(int left, int top, int width, int height,
                              const std::wstring& outputPath,
                              POINT clickMarker, bool drawMarker) {
    if (width <= 0 || height <= 0) return false;

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) return false;

    HDC memDc = CreateCompatibleDC(screenDc);
    if (!memDc) {
        ReleaseDC(nullptr, screenDc);
        return false;
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(screenDc, width, height);
    if (!hBitmap) {
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return false;
    }

    HGDIOBJ oldBitmap = SelectObject(memDc, hBitmap);
    BitBlt(memDc, 0, 0, width, height, screenDc, left, top, SRCCOPY | CAPTUREBLT);

    bool saveOk = false;
    {
        Bitmap bmp(hBitmap, nullptr);
        if (drawMarker) {
            Graphics g(&bmp);
            int cx = clickMarker.x - left;
            int cy = clickMarker.y - top;
            int r = 20;

            // Concentric white and red circles with crosshair lines
            Pen whitePen(Color(255, 255, 255, 255), 8);
            g.DrawEllipse(&whitePen, cx - r, cy - r, r * 2, r * 2);

            Pen redPen(Color(255, 255, 0, 0), 4);
            g.DrawEllipse(&redPen, cx - r, cy - r, r * 2, r * 2);
            g.DrawLine(&redPen, cx - r - 8, cy, cx - r + 2, cy);
            g.DrawLine(&redPen, cx + r - 2, cy, cx + r + 8, cy);
            g.DrawLine(&redPen, cx, cy - r - 8, cx, cy - r + 2);
            g.DrawLine(&redPen, cx, cy + r - 2, cx, cy + r + 8);
        }

        CLSID pngClsid;
        if (GetPngEncoderClsid(pngClsid)) {
            saveOk = (bmp.Save(outputPath.c_str(), &pngClsid, nullptr) == Ok);
        }
    }

    SelectObject(memDc, oldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);

    return saveOk;
}

bool ScreenCapture::ExecuteShot(HWND targetHwnd, long clickX, long clickY,
                               bool fullScreen, bool highlight,
                               const std::wstring& outputPath) {
    RECT rc{};
    bool regionFound = false;

    if (!fullScreen && targetHwnd && GetWindowBounds(targetHwnd, rc)) {
        RECT virtualScreen{
            GetSystemMetrics(SM_XVIRTUALSCREEN),
            GetSystemMetrics(SM_YVIRTUALSCREEN),
            0, 0
        };
        virtualScreen.right = virtualScreen.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        virtualScreen.bottom = virtualScreen.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

        RECT clipped{};
        if (IntersectRect(&clipped, &rc, &virtualScreen) &&
            clipped.right > clipped.left &&
            clipped.bottom > clipped.top) {
            rc = clipped;
            regionFound = true;
        }
    }

    if (!regionFound) {
        POINT pt{ clickX, clickY };
        HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{ sizeof(mi) };
        if (GetMonitorInfoW(monitor, &mi)) {
            rc = mi.rcMonitor;
        } else {
            rc.left = 0;
            rc.top = 0;
            rc.right = GetSystemMetrics(SM_CXSCREEN);
            rc.bottom = GetSystemMetrics(SM_CYSCREEN);
        }
    }

    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    POINT markerPt{ clickX, clickY };
    bool drawMarker = highlight && (clickX != LONG_MIN) &&
        (markerPt.x >= rc.left && markerPt.x < rc.right) &&
        (markerPt.y >= rc.top && markerPt.y < rc.bottom);

    return CaptureArea(rc.left, rc.top, width, height, outputPath, markerPt, drawMarker);
}

} // namespace steprec::core
