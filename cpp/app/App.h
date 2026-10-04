#pragma once

#include "pch.h"
#include <gdiplus.h>

namespace steprec::app {

class CStepRecorderApp : public CWinAppEx {
public:
    CStepRecorderApp();

    virtual BOOL InitInstance() override;
    virtual int ExitInstance() override;

    int RunSelfTest();

private:
    ULONG_PTR m_gdiplusToken{ 0 };
};

extern CStepRecorderApp theApp;

} // namespace steprec::app
