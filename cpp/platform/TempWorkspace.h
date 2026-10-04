#pragma once

#include <string>
#include <vector>

namespace steprec::platform {

class TempWorkspace {
public:
    TempWorkspace();
    explicit TempWorkspace(const std::wstring& customPrefix);
    ~TempWorkspace();

    TempWorkspace(const TempWorkspace&) = delete;
    TempWorkspace& operator=(const TempWorkspace&) = delete;

    TempWorkspace(TempWorkspace&& other) noexcept;
    TempWorkspace& operator=(TempWorkspace&& other) noexcept;

    bool Initialize(std::wstring& errorMessage);
    void Cleanup();

    const std::wstring& GetPath() const { return m_workspacePath; }
    std::wstring GetScreenshotsPath() const;
    std::wstring GetWorkPath() const;
    std::wstring GetArchiveTempPath() const;

    bool IsValid() const { return !m_workspacePath.empty(); }

    static std::wstring GetSystemTempDirectory();
    static bool SafeDeleteDirectoryTree(const std::wstring& rootDir);

private:
    std::wstring m_workspacePath;
    std::wstring m_prefix;
    bool m_ownsDirectory{ false };
};

} // namespace steprec::platform
