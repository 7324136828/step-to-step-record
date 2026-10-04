#pragma once

#include <windows.h>
#include <string>

namespace steprec::platform {

class ProcessRunner {
public:
    static bool OpenFolderAndSelectFile(const std::wstring& filePath);
    static bool OpenFileInDefaultApp(const std::wstring& filePath);
    static std::wstring GetDefaultZipOutputPath();
};

} // namespace steprec::platform
