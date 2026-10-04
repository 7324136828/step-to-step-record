#include "ArchiveService.h"
#include <windows.h>
#include <fstream>
#include <vector>

namespace steprec::services {

bool ArchiveService::ExportArchive(const std::wstring& sourceZip,
                                   const std::wstring& destinationZip,
                                   std::wstring& errorMessage) {
    if (sourceZip.empty() || destinationZip.empty()) {
        errorMessage = L"Invalid source or destination path.";
        return false;
    }

    DWORD attrs = GetFileAttributesW(sourceZip.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        errorMessage = L"Source ZIP file not found: " + sourceZip;
        return false;
    }

    std::wstring tempDest = destinationZip + L".export.tmp";
    if (!CopyFileW(sourceZip.c_str(), tempDest.c_str(), FALSE)) {
        errorMessage = L"Failed to copy archive to temporary destination.";
        return false;
    }

    if (!MoveFileExW(tempDest.c_str(), destinationZip.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tempDest.c_str());
        errorMessage = L"Failed to finalize exported archive.";
        return false;
    }

    return true;
}

bool ArchiveService::ExtractReportToTemp(const std::wstring& zipPath,
                                        std::wstring& outHtmlPath,
                                        std::wstring& errorMessage) {
    std::ifstream zip(zipPath, std::ios::binary);
    if (!zip) {
        errorMessage = L"Cannot open ZIP file: " + zipPath;
        return false;
    }

    wchar_t tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring viewDir = std::wstring(tempDir) + L"StepRecorder\\view_" +
        std::to_wstring(GetCurrentProcessId()) + L"_" +
        std::to_wstring(GetTickCount());
    CreateDirectoryW((std::wstring(tempDir) + L"StepRecorder").c_str(), nullptr);
    CreateDirectoryW(viewDir.c_str(), nullptr);
    CreateDirectoryW((viewDir + L"\\screenshots").c_str(), nullptr);

    bool foundHtml = false;
    outHtmlPath = viewDir + L"\\steps.html";

    // Scan through local headers
    while (zip) {
        uint32_t sig = 0;
        zip.read(reinterpret_cast<char*>(&sig), 4);
        if (!zip || sig != 0x04034b50) break; // Not a local header

        zip.seekg(4, std::ios::cur); // Skip version and flags
        uint16_t method = 0;
        zip.read(reinterpret_cast<char*>(&method), 2);
        zip.seekg(8, std::ios::cur); // Skip mod time/date, crc
        uint32_t compSize = 0;
        zip.read(reinterpret_cast<char*>(&compSize), 4);
        uint32_t uncompSize = 0;
        zip.read(reinterpret_cast<char*>(&uncompSize), 4);
        uint16_t nameLen = 0;
        zip.read(reinterpret_cast<char*>(&nameLen), 2);
        uint16_t extraLen = 0;
        zip.read(reinterpret_cast<char*>(&extraLen), 2);

        std::string name(nameLen, '\0');
        zip.read(&name[0], nameLen);
        if (extraLen > 0) {
            zip.seekg(extraLen, std::ios::cur);
        }

        std::vector<char> data(compSize);
        if (compSize > 0) {
            zip.read(data.data(), compSize);
        }

        if (method == 0) { // STORE method
            std::wstring destFile;
            if (name == "steps.html") {
                destFile = outHtmlPath;
                foundHtml = true;
            } else if (name.rfind("screenshots/", 0) == 0) {
                std::string fname = name.substr(12);
                destFile = viewDir + L"\\screenshots\\" + std::wstring(fname.begin(), fname.end());
            }

            if (!destFile.empty()) {
                std::ofstream out(destFile, std::ios::binary);
                if (out && !data.empty()) {
                    out.write(data.data(), data.size());
                }
            }
        }
    }

    if (!foundHtml) {
        errorMessage = L"steps.html was not found in the archive.";
        return false;
    }

    return true;
}

} // namespace steprec::services
