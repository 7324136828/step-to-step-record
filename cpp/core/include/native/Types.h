#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace steprec::core {

struct Step {
    int n{ 0 };
    std::wstring time;
    std::wstring kind;          // "click", "key", "type"
    std::wstring description;
    std::wstring window;
    std::wstring process;
    std::wstring image;         // e.g. "screenshots/step_001.png"
    std::wstring text;
    HWND hwnd{ nullptr };
};

struct CaptureOptions {
    bool recordClicks{ true };
    bool recordKeys{ true };
    bool highlightClicks{ true };
    bool fullScreen{ false };
    int clickDelayMs{ 400 };
    int keyDelayMs{ 600 };
    uint32_t typingGapMs{ 1200 };
};

struct ShotRequest {
    int stepIndex{ -1 };
    long x{ 0 };
    long y{ 0 };
    HWND hwnd{ nullptr };
};

enum class JobStatus {
    Idle,
    Recording,
    Paused,
    Finalizing,
    Completed,
    Interrupted,
    Failed,
    Discarded
};

inline const wchar_t* JobStatusToString(JobStatus status) {
    switch (status) {
    case JobStatus::Idle: return L"Idle";
    case JobStatus::Recording: return L"Recording";
    case JobStatus::Paused: return L"Paused";
    case JobStatus::Finalizing: return L"Saving ZIP...";
    case JobStatus::Completed: return L"Completed";
    case JobStatus::Interrupted: return L"Interrupted";
    case JobStatus::Failed: return L"Failed";
    case JobStatus::Discarded: return L"Discarded";
    default: return L"Unknown";
    }
}

inline JobStatus StringToJobStatus(const std::wstring& str) {
    if (str == L"Recording") return JobStatus::Recording;
    if (str == L"Paused") return JobStatus::Paused;
    if (str == L"Finalizing" || str == L"Saving ZIP...") return JobStatus::Finalizing;
    if (str == L"Completed") return JobStatus::Completed;
    if (str == L"Interrupted") return JobStatus::Interrupted;
    if (str == L"Failed") return JobStatus::Failed;
    if (str == L"Discarded") return JobStatus::Discarded;
    return JobStatus::Idle;
}

struct JobMetadata {
    std::wstring id;
    std::wstring timestamp;
    int stepCount{ 0 };
    int durationSeconds{ 0 };
    std::wstring status{ L"Completed" };
    std::wstring targetZipPath;
    std::wstring errorMessage;
};

// String conversion helpers
inline std::string Utf8Encode(const std::wstring& w) {
    if (w.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string s(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), size, nullptr, nullptr);
    return s;
}

inline std::wstring Utf8Decode(const std::string& s) {
    if (s.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring w(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), size);
    return w;
}

inline std::wstring EscapeHtml(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 16);
    for (wchar_t c : s) {
        switch (c) {
        case L'<': out += L"&lt;"; break;
        case L'>': out += L"&gt;"; break;
        case L'&': out += L"&amp;"; break;
        case L'"': out += L"&quot;"; break;
        case L'\'': out += L"&#39;"; break;
        default: out += c; break;
        }
    }
    return out;
}

inline std::string EscapeJsonString(const std::wstring& w) {
    std::string out = "\"";
    std::string utf8 = Utf8Encode(w);
    for (char c : utf8) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char b[8];
                sprintf_s(b, "\\u%04x", static_cast<unsigned char>(c));
                out += b;
            } else {
                out += c;
            }
            break;
        }
    }
    out += "\"";
    return out;
}

} // namespace steprec::core
