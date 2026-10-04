#include "TestHarness.h"
#include "TempWorkspace.h"
#include <iostream>
#include <fstream>
#include <windows.h>

TEST_CASE(Workspace_InitializeAndCleanup) {
    std::wstring err;
    std::wstring createdPath;
    {
        steprec::platform::TempWorkspace ws(L"unittest");
        ASSERT_TRUE(ws.Initialize(err));
        ASSERT_TRUE(ws.IsValid());

        createdPath = ws.GetPath();
        ASSERT_FALSE(createdPath.empty());

        DWORD attrs = GetFileAttributesW(createdPath.c_str());
        ASSERT_NE(attrs, INVALID_FILE_ATTRIBUTES);
        ASSERT_TRUE(attrs & FILE_ATTRIBUTE_DIRECTORY);

        // Check subdirectories
        DWORD shotAttrs = GetFileAttributesW(ws.GetScreenshotsPath().c_str());
        ASSERT_NE(shotAttrs, INVALID_FILE_ATTRIBUTES);

        // Create a dummy file inside workspace
        std::wstring testFile = ws.GetWorkPath() + L"\\scratch.txt";
        std::ofstream f(testFile);
        f << "temp data";
        f.close();
        ASSERT_NE(GetFileAttributesW(testFile.c_str()), INVALID_FILE_ATTRIBUTES);

        // ws goes out of scope here -> Cleanup() is called
    }

    // After RAII destruction, workspace directory and all contents must be deleted
    DWORD afterAttrs = GetFileAttributesW(createdPath.c_str());
    ASSERT_EQ(afterAttrs, INVALID_FILE_ATTRIBUTES);

    return true;
}

TEST_CASE(Workspace_SecurityBoundaryCheck) {
    // Attempting to delete unsafe or root paths must be rejected by boundary checks
    ASSERT_FALSE(steprec::platform::TempWorkspace::SafeDeleteDirectoryTree(L""));
    ASSERT_FALSE(steprec::platform::TempWorkspace::SafeDeleteDirectoryTree(L"C:\\"));
    ASSERT_FALSE(steprec::platform::TempWorkspace::SafeDeleteDirectoryTree(L"C:\\Windows"));
    return true;
}
