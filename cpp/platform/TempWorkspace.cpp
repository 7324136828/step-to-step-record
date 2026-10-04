#include "TempWorkspace.h"
#include <windows.h>
#include <random>

namespace steprec::platform {

namespace {

typedef DWORD (WINAPI *PFN_GetTempPath2W)(DWORD, LPWSTR);

std::wstring GenerateUnpredictableId() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    wchar_t buf[32] = { 0 };
    swprintf_s(buf, L"%016llx", dis(gen));
    return buf;
}

} // anonymous namespace

TempWorkspace::TempWorkspace()
    : m_prefix(L"steprec") {}

TempWorkspace::TempWorkspace(const std::wstring& customPrefix)
    : m_prefix(customPrefix) {}

TempWorkspace::~TempWorkspace() {
    Cleanup();
}

TempWorkspace::TempWorkspace(TempWorkspace&& other) noexcept
    : m_workspacePath(std::move(other.m_workspacePath)),
      m_prefix(std::move(other.m_prefix)),
      m_ownsDirectory(other.m_ownsDirectory) {
    other.m_ownsDirectory = false;
    other.m_workspacePath.clear();
}

TempWorkspace& TempWorkspace::operator=(TempWorkspace&& other) noexcept {
    if (this != &other) {
        Cleanup();
        m_workspacePath = std::move(other.m_workspacePath);
        m_prefix = std::move(other.m_prefix);
        m_ownsDirectory = other.m_ownsDirectory;
        other.m_ownsDirectory = false;
        other.m_workspacePath.clear();
    }
    return *this;
}

std::wstring TempWorkspace::GetSystemTempDirectory() {
    wchar_t tempBuffer[MAX_PATH + 1] = { 0 };
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (hKernel32) {
        auto pfnGetTempPath2W = reinterpret_cast<PFN_GetTempPath2W>(GetProcAddress(hKernel32, "GetTempPath2W"));
        if (pfnGetTempPath2W) {
            DWORD len = pfnGetTempPath2W(MAX_PATH, tempBuffer);
            if (len > 0 && len < MAX_PATH) {
                return tempBuffer;
            }
        }
    }

    DWORD len = GetTempPathW(MAX_PATH, tempBuffer);
    if (len > 0 && len < MAX_PATH) {
        return tempBuffer;
    }
    return L"C:\\Temp\\";
}

bool TempWorkspace::Initialize(std::wstring& errorMessage) {
    Cleanup();

    std::wstring sysTemp = GetSystemTempDirectory();
    DWORD attrs = GetFileAttributesW(sysTemp.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        errorMessage = L"System temporary directory is inaccessible: " + sysTemp;
        return false;
    }

    std::wstring baseDir = sysTemp + L"StepRecorder\\jobs\\";
    CreateDirectoryW((sysTemp + L"StepRecorder").c_str(), nullptr);
    CreateDirectoryW(baseDir.c_str(), nullptr);

    std::wstring folderName = m_prefix + L"_" +
        std::to_wstring(GetCurrentProcessId()) + L"_" +
        std::to_wstring(GetTickCount()) + L"_" +
        GenerateUnpredictableId();

    m_workspacePath = baseDir + folderName;
    if (!CreateDirectoryW(m_workspacePath.c_str(), nullptr)) {
        errorMessage = L"Failed to create workspace directory: " + m_workspacePath;
        m_workspacePath.clear();
        return false;
    }

    m_ownsDirectory = true;
    CreateDirectoryW(GetScreenshotsPath().c_str(), nullptr);
    CreateDirectoryW(GetWorkPath().c_str(), nullptr);
    CreateDirectoryW(GetArchiveTempPath().c_str(), nullptr);

    return true;
}

void TempWorkspace::Cleanup() {
    if (m_ownsDirectory && !m_workspacePath.empty()) {
        SafeDeleteDirectoryTree(m_workspacePath);
        m_ownsDirectory = false;
        m_workspacePath.clear();
    }
}

std::wstring TempWorkspace::GetScreenshotsPath() const {
    return m_workspacePath + L"\\screenshots";
}

std::wstring TempWorkspace::GetWorkPath() const {
    return m_workspacePath + L"\\work";
}

std::wstring TempWorkspace::GetArchiveTempPath() const {
    return m_workspacePath + L"\\archive";
}

bool TempWorkspace::SafeDeleteDirectoryTree(const std::wstring& rootDir) {
    if (rootDir.empty() || rootDir.length() < 8) return false;

    // Boundary check: ensure target path is within temporary directory or StepRecorder\jobs
    std::wstring sysTemp = GetSystemTempDirectory();
    if (_wcsnicmp(rootDir.c_str(), sysTemp.c_str(), sysTemp.length()) != 0) {
        return false;
    }

    std::wstring searchPattern = rootDir + L"\\*.*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
                continue;
            }
            std::wstring itemPath = rootDir + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                SafeDeleteDirectoryTree(itemPath);
            } else {
                DeleteFileW(itemPath.c_str());
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    return RemoveDirectoryW(rootDir.c_str()) != FALSE;
}

} // namespace steprec::platform
