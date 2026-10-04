#pragma once

#include "pch.h"
#include "WorkPage.h"
#include "HistoryPage.h"

namespace steprec::app {

class CMainDialog : public CDialogEx {
public:
    CMainDialog(CWnd* pParent = nullptr);

    enum { IDD = IDD_MAIN_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnClickedSettings();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnCancel() override;
    afx_msg void OnOK() override;

    afx_msg LRESULT OnAppStepRecorded(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAppStatusChanged(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnAppStateChanged(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    void RepositionChildPages();

    CTabCtrl m_tabMain;
    CWorkPage m_workPage;
    CHistoryPage m_historyPage;
    HICON m_hIcon{ nullptr };
    bool m_closeInProgress{ false };

    int m_clickDelayMs{ 400 };
    int m_keyDelayMs{ 600 };
    int m_typingGapMs{ 1200 };
};

} // namespace steprec::app
