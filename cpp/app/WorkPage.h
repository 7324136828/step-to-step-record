#pragma once

#include "pch.h"
#include "native/Types.h"

namespace steprec::app {

class CWorkPage : public CDialogEx {
public:
    CWorkPage(CWnd* pParent = nullptr);

    enum { IDD = IDD_WORK_PAGE };

    void AddStepToUI(const core::Step& step);
    void UpdateStatusUI(const std::wstring& status);
    void UpdateStateUI(core::JobStatus state);

    void SetDelays(int clickDelayMs, int keyDelayMs, int typingGapMs);

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnBnClickedBrowse();
    afx_msg void OnBnClickedStart();
    afx_msg void OnBnClickedStop();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDropFiles(HDROP hDropInfo);

    DECLARE_MESSAGE_MAP()

private:
    core::CaptureOptions GetOptionsFromUI() const;

    CEdit m_editPath;
    CButton m_btnBrowse;
    CButton m_chkClicks;
    CButton m_chkKeys;
    CButton m_chkHighlight;
    CButton m_chkFullScreen;
    CButton m_btnStart;
    CButton m_btnStop;
    CStatic m_staticCount;
    CStatic m_staticStatus;
    CProgressCtrl m_progress;
    CListCtrl m_listSteps;

    int m_clickDelayMs{ 400 };
    int m_keyDelayMs{ 600 };
    int m_typingGapMs{ 1200 };
};

} // namespace steprec::app
