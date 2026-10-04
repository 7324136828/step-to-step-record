#include "pch.h"
#include "MainDialog.h"
#include "SettingsDialog.h"
#include "JobManager.h"

namespace steprec::app {

BEGIN_MESSAGE_MAP(CMainDialog, CDialogEx)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_MAIN, &CMainDialog::OnTcnSelchangeTab)
    ON_BN_CLICKED(IDC_BTN_SETTINGS, &CMainDialog::OnBnClickedSettings)
    ON_WM_SIZE()
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_MESSAGE(WM_APP_STEP_RECORDED, &CMainDialog::OnAppStepRecorded)
    ON_MESSAGE(WM_APP_STATUS_CHANGED, &CMainDialog::OnAppStatusChanged)
    ON_MESSAGE(WM_APP_STATE_CHANGED, &CMainDialog::OnAppStateChanged)
END_MESSAGE_MAP()

CMainDialog::CMainDialog(CWnd* pParent)
    : CDialogEx(IDD_MAIN_DIALOG, pParent) {}

void CMainDialog::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TAB_MAIN, m_tabMain);
}

BOOL CMainDialog::OnInitDialog() {
    CDialogEx::OnInitDialog();

    m_hIcon = AfxGetApp()->LoadIconW(IDR_MAINFRAME);
    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    // Insert tabs
    m_tabMain.InsertItem(0, L"Record");
    m_tabMain.InsertItem(1, L"History");

    // Keep DS_CONTROL pages directly under the dialog. A tab control is not
    // a dialog-navigation container: nesting pages under it makes focused
    // page controls unreachable to IsDialogMessage/GetNextDlgTabItem.
    m_workPage.Create(IDD_WORK_PAGE, this);
    m_historyPage.Create(IDD_HISTORY_PAGE, this);

    RepositionChildPages();

    m_workPage.ShowWindow(SW_SHOW);
    m_historyPage.ShowWindow(SW_HIDE);

    // Initialize JobManager with this window as notification target
    services::JobManager::Instance().Initialize(m_hWnd);

    services::JobManager::Instance().SetCallbacks(
        [this](const core::Step& step) {
            auto stepMessage = std::make_unique<core::Step>(step);
            if (PostMessage(WM_APP_STEP_RECORDED, reinterpret_cast<WPARAM>(stepMessage.get()), 0)) {
                stepMessage.release();
            }
        },
        [this](const std::wstring& status) {
            auto statusMessage = std::make_unique<std::wstring>(status);
            if (PostMessage(WM_APP_STATUS_CHANGED, reinterpret_cast<WPARAM>(statusMessage.get()), 0)) {
                statusMessage.release();
            }
        },
        [this](core::JobStatus state) {
            PostMessage(WM_APP_STATE_CHANGED, static_cast<WPARAM>(state), 0);
        }
    );

    return TRUE;
}

void CMainDialog::RepositionChildPages() {
    if (!m_tabMain.GetSafeHwnd()) return;

    CRect rectTab;
    m_tabMain.GetClientRect(&rectTab);
    m_tabMain.AdjustRect(FALSE, &rectTab);
    m_tabMain.MapWindowPoints(this, &rectTab);

    if (m_workPage.GetSafeHwnd()) {
        m_workPage.MoveWindow(&rectTab);
    }
    if (m_historyPage.GetSafeHwnd()) {
        m_historyPage.MoveWindow(&rectTab);
    }
}

void CMainDialog::OnTcnSelchangeTab(NMHDR* /*pNMHDR*/, LRESULT* pResult) {
    int sel = m_tabMain.GetCurSel();
    if (sel == 0) {
        m_workPage.ShowWindow(SW_SHOW);
        m_historyPage.ShowWindow(SW_HIDE);
    } else {
        m_workPage.ShowWindow(SW_HIDE);
        m_historyPage.ShowWindow(SW_SHOW);
        m_historyPage.RefreshList();
    }
    *pResult = 0;
}

void CMainDialog::OnBnClickedSettings() {
    CSettingsDialog dlg(this);
    dlg.m_clickDelayMs = m_clickDelayMs;
    dlg.m_keyDelayMs = m_keyDelayMs;
    dlg.m_typingGapMs = m_typingGapMs;

    if (dlg.DoModal() == IDOK) {
        m_clickDelayMs = dlg.m_clickDelayMs;
        m_keyDelayMs = dlg.m_keyDelayMs;
        m_typingGapMs = dlg.m_typingGapMs;
        m_workPage.SetDelays(m_clickDelayMs, m_keyDelayMs, m_typingGapMs);
    }
}

void CMainDialog::OnSize(UINT nType, int cx, int cy) {
    CDialogEx::OnSize(nType, cx, cy);

    if (!m_tabMain.GetSafeHwnd()) return;

    CRect rectClient;
    GetClientRect(&rectClient);

    // Resize tab control to fit main window minus bottom buttons
    int tabW = rectClient.Width() - 14;
    int tabH = rectClient.Height() - 44;
    if (tabW > 100 && tabH > 100) {
        m_tabMain.SetWindowPos(nullptr, 7, 7, tabW, tabH, SWP_NOZORDER);
        RepositionChildPages();

        CWnd* pBtnSettings = GetDlgItem(IDC_BTN_SETTINGS);
        if (pBtnSettings && pBtnSettings->GetSafeHwnd()) {
            pBtnSettings->SetWindowPos(nullptr, 7, rectClient.bottom - 28, 65, 22, SWP_NOZORDER);
        }

        CWnd* pBtnClose = GetDlgItem(IDOK);
        if (pBtnClose && pBtnClose->GetSafeHwnd()) {
            pBtnClose->SetWindowPos(nullptr, rectClient.right - 72, rectClient.bottom - 28, 65, 22, SWP_NOZORDER);
        }
    }
}

void CMainDialog::OnTimer(UINT_PTR nIDEvent) {
    services::JobManager::Instance().HandleTimer(nIDEvent);
    CDialogEx::OnTimer(nIDEvent);
}

void CMainDialog::OnClose() {
    if (m_closeInProgress) return;
    m_closeInProgress = true;

    if (services::JobManager::Instance().IsActive()) {
        int ans = AfxMessageBox(
            L"A recording session is currently active.\n\n"
            L"Click YES to stop and save the recording before closing,\n"
            L"NO to discard the recording and exit immediately,\n"
            L"or CANCEL to continue recording.",
            MB_YESNOCANCEL | MB_ICONQUESTION
        );

        if (ans == IDCANCEL) {
            m_closeInProgress = false;
            return;
        } else if (ans == IDYES) {
            std::wstring err;
            if (!services::JobManager::Instance().StopSession(err)) {
                AfxMessageBox(
                    (L"The recording could not be saved.\n\n" + err).c_str(),
                    MB_OK | MB_ICONERROR
                );
                m_closeInProgress = false;
                return;
            }
        } else {
            services::JobManager::Instance().CancelSession();
        }
    }

    // The default WM_CLOSE handler sends IDCANCEL, which calls our OnCancel
    // override and would re-enter OnClose. End the modal dialog directly.
    CDialogEx::OnCancel();
}

void CMainDialog::OnDestroy() {
    services::JobManager::Instance().SetCallbacks({}, {}, {});

    // Stop/save can leave notifications in the queue after the dialog ends.
    // Their payloads belong to this window and will no longer be dispatched.
    MSG message{};
    while (::PeekMessageW(&message, m_hWnd, WM_APP_STEP_RECORDED, WM_APP_STEP_RECORDED, PM_REMOVE)) {
        delete reinterpret_cast<core::Step*>(message.wParam);
    }
    while (::PeekMessageW(&message, m_hWnd, WM_APP_STATUS_CHANGED, WM_APP_STATUS_CHANGED, PM_REMOVE)) {
        delete reinterpret_cast<std::wstring*>(message.wParam);
    }

    CDialogEx::OnDestroy();
}

void CMainDialog::OnCancel() {
    OnClose();
}

void CMainDialog::OnOK() {
    OnClose();
}

LRESULT CMainDialog::OnAppStepRecorded(WPARAM wParam, LPARAM /*lParam*/) {
    auto* pStep = reinterpret_cast<core::Step*>(wParam);
    if (pStep) {
        m_workPage.AddStepToUI(*pStep);
        delete pStep;
    }
    return 0;
}

LRESULT CMainDialog::OnAppStatusChanged(WPARAM wParam, LPARAM /*lParam*/) {
    auto* pStr = reinterpret_cast<std::wstring*>(wParam);
    if (pStr) {
        m_workPage.UpdateStatusUI(*pStr);
        delete pStr;
    }
    return 0;
}

LRESULT CMainDialog::OnAppStateChanged(WPARAM wParam, LPARAM /*lParam*/) {
    auto state = static_cast<core::JobStatus>(wParam);
    m_workPage.UpdateStateUI(state);
    return 0;
}

} // namespace steprec::app
