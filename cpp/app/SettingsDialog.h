#pragma once

#include "pch.h"

namespace steprec::app {

class CSettingsDialog : public CDialogEx {
public:
    CSettingsDialog(CWnd* pParent = nullptr);

    enum { IDD = IDD_SETTINGS_DIALOG };

    int m_clickDelayMs{ 400 };
    int m_keyDelayMs{ 600 };
    int m_typingGapMs{ 1200 };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void OnOK() override;

    DECLARE_MESSAGE_MAP()

private:
    CEdit m_editClickDelay;
    CEdit m_editKeyDelay;
    CEdit m_editTypingGap;
};

} // namespace steprec::app
