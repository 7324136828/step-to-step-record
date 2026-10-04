#pragma once

#include "pch.h"
#include "native/Types.h"
#include <vector>

namespace steprec::app {

class CHistoryPage : public CDialogEx {
public:
    CHistoryPage(CWnd* pParent = nullptr);

    enum { IDD = IDD_HISTORY_PAGE };

    void RefreshList();

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnBnClickedOpenZip();
    afx_msg void OnBnClickedOpenFolder();
    afx_msg void OnBnClickedViewReport();
    afx_msg void OnBnClickedDeleteRecord();
    afx_msg void OnBnClickedClearHistory();
    afx_msg void OnBnClickedRefresh();
    afx_msg void OnDblClkList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);

    DECLARE_MESSAGE_MAP()

private:
    int GetSelectedIndex() const;
    std::wstring GetSelectedZipPath() const;

    CListCtrl m_listHistory;
    CButton m_btnOpenZip;
    CButton m_btnOpenFolder;
    CButton m_btnViewReport;
    CButton m_btnDeleteRecord;
    CButton m_btnClearHistory;
    CButton m_btnRefresh;

    std::vector<core::JobMetadata> m_cachedRecords;
};

} // namespace steprec::app
