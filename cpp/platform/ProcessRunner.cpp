#include "ProcessRunner.h"
#include <shlobj.h>
#include <shellapi.h>
#include <knownfolders.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace steprec::platform {

bool ProcessRunner::OpenFolderAndSelectFile(const std::wstring& filePath) {
    if (filePath.empty()) return false;

    // Verify file or folder exists
    DWORD attrs = GetFileAttributesW(filePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;

    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(filePath.c_str());
    if (!pidl) return false;

    HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
    ILFree(pidl);

    if (SUCCEEDED(hr)) {
        return true;
    }

    // Fallback using ShellExecuteExW with explorer.exe /select,"path"
    std::wstring params = L"/select,\"" + filePath + L"\"";
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"open";
    sei.lpFile = L"explorer.exe";
    sei.lpParameters = params.c_str();
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

bool ProcessRunner::OpenFileInDefaultApp(const std::wstring& filePath) {
    if (filePath.empty()) return false;
    DWORD attrs = GetFileAttributesW(filePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"open";
    sei.lpFile = filePath.c_str();
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

std::wstring ProcessRunner::GetDefaultZipOutputPath() {
    for (const KNOWNFOLDERID* folderId : { &FOLDERID_Desktop, &FOLDERID_Documents }) {
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(*folderId, 0, nullptr, &path))) {
            std::wstring dir(path);
            CoTaskMemFree(path);
            DWORD attrs = GetFileAttributesW(dir.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                return dir + L"\\steps.zip";
            }
        }
    }

    wchar_t tempBuf[MAX_PATH];
    DWORD len = GetTempPathW(MAX_PATH, tempBuf);
    if (len > 0 && len < MAX_PATH) {
        return std::wstring(tempBuf) + L"steps.zip";
    }
    return L"steps.zip";
}

} // namespace steprec::platform
