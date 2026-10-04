#include "pch.h"
#include "HistoryPage.h"
#include "HistoryStore.h"
#include "ProcessRunner.h"
#include "ArchiveService.h"

namespace steprec::app {

BEGIN_MESSAGE_MAP(CHistoryPage, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_OPEN_ZIP, &CHistoryPage::OnBnClickedOpenZip)
    ON_BN_CLICKED(IDC_BTN_OPEN_FOLDER, &CHistoryPage::OnBnClickedOpenFolder)
    ON_BN_CLICKED(IDC_BTN_VIEW_REPORT, &CHistoryPage::OnBnClickedViewReport)
    ON_BN_CLICKED(IDC_BTN_DELETE_RECORD, &CHistoryPage::OnBnClickedDeleteRecord)
    ON_BN_CLICKED(IDC_BTN_CLEAR_HISTORY, &CHistoryPage::OnBnClickedClearHistory)
    ON_BN_CLICKED(IDC_BTN_REFRESH, &CHistoryPage::OnBnClickedRefresh)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST_HISTORY, &CHistoryPage::OnDblClkList)
    ON_WM_SIZE()
END_MESSAGE_MAP()

CHistoryPage::CHistoryPage(CWnd* pParent)
    : CDialogEx(IDD_HISTORY_PAGE, pParent) {}

void CHistoryPage::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_HISTORY, m_listHistory);
    DDX_Control(pDX, IDC_BTN_OPEN_ZIP, m_btnOpenZip);
    DDX_Control(pDX, IDC_BTN_OPEN_FOLDER, m_btnOpenFolder);
    DDX_Control(pDX, IDC_BTN_VIEW_REPORT, m_btnViewReport);
    DDX_Control(pDX, IDC_BTN_DELETE_RECORD, m_btnDeleteRecord);
    DDX_Control(pDX, IDC_BTN_CLEAR_HISTORY, m_btnClearHistory);
    DDX_Control(pDX, IDC_BTN_REFRESH, m_btnRefresh);
}

BOOL CHistoryPage::OnInitDialog() {
    CDialogEx::OnInitDialog();

    m_listHistory.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    m_listHistory.InsertColumn(0, L"#", LVCFMT_RIGHT, 35);
    m_listHistory.InsertColumn(1, L"Timestamp", LVCFMT_LEFT, 130);
    m_listHistory.InsertColumn(2, L"Steps", LVCFMT_RIGHT, 50);
    m_listHistory.InsertColumn(3, L"Duration", LVCFMT_RIGHT, 65);
    m_listHistory.InsertColumn(4, L"Status", LVCFMT_LEFT, 85);
    m_listHistory.InsertColumn(5, L"Target ZIP Path", LVCFMT_LEFT, 280);

    RefreshList();

    return TRUE;
}

void CHistoryPage::RefreshList() {
    if (!m_listHistory.GetSafeHwnd()) return;

    m_listHistory.DeleteAllItems();
    m_cachedRecords = services::HistoryStore::Instance().GetRecords();

    for (size_t i = 0; i < m_cachedRecords.size(); ++i) {
        const auto& r = m_cachedRecords[i];
        int itemIndex = static_cast<int>(i);

        int inserted = m_listHistory.InsertItem(itemIndex, std::to_wstring(i + 1).c_str());
        m_listHistory.SetItemText(inserted, 1, r.timestamp.c_str());
        m_listHistory.SetItemText(inserted, 2, std::to_wstring(r.stepCount).c_str());

        wchar_t durBuf[32] = { 0 };
        swprintf_s(durBuf, L"%ds", r.durationSeconds);
        m_listHistory.SetItemText(inserted, 3, durBuf);

        // Check file presence
        std::wstring statusDisplay = r.status;
        if (!r.targetZipPath.empty()) {
            DWORD attrs = GetFileAttributesW(r.targetZipPath.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES && r.status == L"Completed") {
                statusDisplay += L" (Missing)";
            }
        }
        m_listHistory.SetItemText(inserted, 4, statusDisplay.c_str());
        m_listHistory.SetItemText(inserted, 5, r.targetZipPath.c_str());

        m_listHistory.SetItemData(inserted, static_cast<DWORD_PTR>(i));
    }
}

int CHistoryPage::GetSelectedIndex() const {
    POSITION pos = m_listHistory.GetFirstSelectedItemPosition();
    if (!pos) return -1;
    return m_listHistory.GetNextSelectedItem(pos);
}

std::wstring CHistoryPage::GetSelectedZipPath() const {
    int sel = GetSelectedIndex();
    if (sel < 0 || sel >= static_cast<int>(m_cachedRecords.size())) return L"";
    return m_cachedRecords[sel].targetZipPath;
}

void CHistoryPage::OnBnClickedOpenZip() {
    std::wstring zipPath = GetSelectedZipPath();
    if (zipPath.empty()) {
        AfxMessageBox(L"Please select a recording from the history list.", MB_ICONINFORMATION);
        return;
    }
    if (!platform::ProcessRunner::OpenFileInDefaultApp(zipPath)) {
        AfxMessageBox((L"Could not open archive. File may have been moved or deleted:\n" + zipPath).c_str(), MB_ICONWARNING);
    }
}

void CHistoryPage::OnBnClickedOpenFolder() {
    std::wstring zipPath = GetSelectedZipPath();
    if (zipPath.empty()) {
        AfxMessageBox(L"Please select a recording from the history list.", MB_ICONINFORMATION);
        return;
    }
    if (!platform::ProcessRunner::OpenFolderAndSelectFile(zipPath)) {
        AfxMessageBox((L"Could not locate archive on disk:\n" + zipPath).c_str(), MB_ICONWARNING);
    }
}

void CHistoryPage::OnBnClickedViewReport() {
    std::wstring zipPath = GetSelectedZipPath();
    if (zipPath.empty()) {
        AfxMessageBox(L"Please select a recording from the history list.", MB_ICONINFORMATION);
        return;
    }

    std::wstring htmlPath;
    std::wstring err;
    if (services::ArchiveService::ExtractReportToTemp(zipPath, htmlPath, err)) {
        platform::ProcessRunner::OpenFileInDefaultApp(htmlPath);
    } else {
        AfxMessageBox((L"Failed to extract HTML report from archive:\n" + err).c_str(), MB_ICONERROR);
    }
}

void CHistoryPage::OnBnClickedDeleteRecord() {
    int sel = GetSelectedIndex();
    if (sel < 0 || sel >= static_cast<int>(m_cachedRecords.size())) {
        AfxMessageBox(L"Please select a recording to delete.", MB_ICONINFORMATION);
        return;
    }

    const auto& rec = m_cachedRecords[sel];
    if (AfxMessageBox(L"Remove this entry from the recording history? (The ZIP file on disk will not be deleted)",
                      MB_YESNO | MB_ICONQUESTION) == IDYES) {
        services::HistoryStore::Instance().RemoveRecord(rec.id);
        RefreshList();
    }
}

void CHistoryPage::OnBnClickedClearHistory() {
    if (m_cachedRecords.empty()) return;

    if (AfxMessageBox(L"Are you sure you want to clear all recording history records?",
                      MB_YESNO | MB_ICONWARNING) == IDYES) {
        services::HistoryStore::Instance().ClearAll();
        RefreshList();
    }
}

void CHistoryPage::OnBnClickedRefresh() {
    services::HistoryStore::Instance().Load();
    RefreshList();
}

void CHistoryPage::OnDblClkList(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    OnBnClickedViewReport();
    *pResult = 0;
}

void CHistoryPage::OnSize(UINT nType, int cx, int cy) {
    CDialogEx::OnSize(nType, cx, cy);

    if (!m_listHistory.GetSafeHwnd()) return;

    CRect rectClient;
    GetClientRect(&rectClient);

    CRect listRect;
    m_listHistory.GetWindowRect(&listRect);
    ScreenToClient(&listRect);

    int newHeight = rectClient.bottom - listRect.top - 36;
    int newWidth = rectClient.right - listRect.left - 10;
    if (newHeight > 50 && newWidth > 100) {
        m_listHistory.SetWindowPos(nullptr, 0, 0, newWidth, newHeight, SWP_NOMOVE | SWP_NOZORDER);

        // Move action buttons to bottom
        int btnY = rectClient.bottom - 26;
        int btnH = 22;
        int spacing = 8;
        int curX = listRect.left;

        auto repositionBtn = [&](CButton& btn, int width) {
            if (btn.GetSafeHwnd()) {
                btn.SetWindowPos(nullptr, curX, btnY, width, btnH, SWP_NOZORDER);
                curX += width + spacing;
            }
        };

        repositionBtn(m_btnOpenZip, 65);
        repositionBtn(m_btnOpenFolder, 75);
        repositionBtn(m_btnViewReport, 100);
        repositionBtn(m_btnDeleteRecord, 85);
        repositionBtn(m_btnClearHistory, 65);

        if (m_btnRefresh.GetSafeHwnd()) {
            m_btnRefresh.SetWindowPos(nullptr, rectClient.right - 70, btnY, 60, btnH, SWP_NOZORDER);
        }
    }
}

} // namespace steprec::app
