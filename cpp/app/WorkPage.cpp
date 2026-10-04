#include "pch.h"
#include "WorkPage.h"
#include "JobManager.h"
#include "ProcessRunner.h"

namespace steprec::app {

BEGIN_MESSAGE_MAP(CWorkPage, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_BROWSE, &CWorkPage::OnBnClickedBrowse)
    ON_BN_CLICKED(IDC_BTN_START, &CWorkPage::OnBnClickedStart)
    ON_BN_CLICKED(IDC_BTN_STOP, &CWorkPage::OnBnClickedStop)
    ON_WM_SIZE()
    ON_WM_DROPFILES()
END_MESSAGE_MAP()

CWorkPage::CWorkPage(CWnd* pParent)
    : CDialogEx(IDD_WORK_PAGE, pParent) {}

void CWorkPage::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_PATH, m_editPath);
    DDX_Control(pDX, IDC_BTN_BROWSE, m_btnBrowse);
    DDX_Control(pDX, IDC_CHK_CLICKS, m_chkClicks);
    DDX_Control(pDX, IDC_CHK_KEYS, m_chkKeys);
    DDX_Control(pDX, IDC_CHK_HIGHLIGHT, m_chkHighlight);
    DDX_Control(pDX, IDC_CHK_FULLSCREEN, m_chkFullScreen);
    DDX_Control(pDX, IDC_BTN_START, m_btnStart);
    DDX_Control(pDX, IDC_BTN_STOP, m_btnStop);
    DDX_Control(pDX, IDC_STATIC_COUNT, m_staticCount);
    DDX_Control(pDX, IDC_STATIC_STATUS, m_staticStatus);
    DDX_Control(pDX, IDC_PROGRESS_RECORDING, m_progress);
    DDX_Control(pDX, IDC_LIST_STEPS, m_listSteps);
}

BOOL CWorkPage::OnInitDialog() {
    CDialogEx::OnInitDialog();

    DragAcceptFiles(TRUE);

    // Initial checkbox states
    m_chkClicks.SetCheck(BST_CHECKED);
    m_chkKeys.SetCheck(BST_CHECKED);
    m_chkHighlight.SetCheck(BST_CHECKED);
    m_chkFullScreen.SetCheck(BST_UNCHECKED);

    // Default output path
    std::wstring defaultPath = platform::ProcessRunner::GetDefaultZipOutputPath();
    m_editPath.SetWindowTextW(defaultPath.c_str());

    // Setup steps list control
    m_listSteps.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    m_listSteps.InsertColumn(0, L"#", LVCFMT_RIGHT, 40);
    m_listSteps.InsertColumn(1, L"Time", LVCFMT_LEFT, 75);
    m_listSteps.InsertColumn(2, L"Type", LVCFMT_LEFT, 60);
    m_listSteps.InsertColumn(3, L"Action Details", LVCFMT_LEFT, 320);
    m_listSteps.InsertColumn(4, L"Window", LVCFMT_LEFT, 180);

    m_progress.SetRange(0, 100);
    m_progress.SetPos(0);

    return TRUE;
}

core::CaptureOptions CWorkPage::GetOptionsFromUI() const {
    core::CaptureOptions opts;
    opts.recordClicks = (m_chkClicks.GetCheck() == BST_CHECKED);
    opts.recordKeys = (m_chkKeys.GetCheck() == BST_CHECKED);
    opts.highlightClicks = (m_chkHighlight.GetCheck() == BST_CHECKED);
    opts.fullScreen = (m_chkFullScreen.GetCheck() == BST_CHECKED);
    opts.clickDelayMs = m_clickDelayMs;
    opts.keyDelayMs = m_keyDelayMs;
    opts.typingGapMs = static_cast<uint32_t>(m_typingGapMs);
    return opts;
}

void CWorkPage::SetDelays(int clickDelayMs, int keyDelayMs, int typingGapMs) {
    m_clickDelayMs = clickDelayMs;
    m_keyDelayMs = keyDelayMs;
    m_typingGapMs = typingGapMs;
}

void CWorkPage::OnBnClickedBrowse() {
    CString currentPath;
    m_editPath.GetWindowTextW(currentPath);

    CFileDialog fileDlg(FALSE, L"zip", currentPath.IsEmpty() ? L"steps.zip" : currentPath.GetString(),
                        OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
                        L"ZIP Archives (*.zip)|*.zip|All Files (*.*)|*.*||", this);

    if (fileDlg.DoModal() == IDOK) {
        m_editPath.SetWindowTextW(fileDlg.GetPathName());
    }
}

void CWorkPage::OnBnClickedStart() {
    CString pathStr;
    m_editPath.GetWindowTextW(pathStr);
    std::wstring zipPath = pathStr.GetString();

    if (zipPath.empty()) {
        AfxMessageBox(L"Please enter or select a valid destination ZIP path.", MB_ICONWARNING);
        return;
    }

    core::CaptureOptions options = GetOptionsFromUI();
    if (!options.recordClicks && !options.recordKeys) {
        AfxMessageBox(L"Please enable at least one capture option (mouse clicks or keyboard input).", MB_ICONWARNING);
        return;
    }

    m_listSteps.DeleteAllItems();
    m_staticCount.SetWindowTextW(L"Steps recorded: 0");

    std::wstring error;
    bool ok = services::JobManager::Instance().StartSession(zipPath, options, error);
    if (!ok) {
        AfxMessageBox((L"Failed to start recording:\n" + error).c_str(), MB_ICONERROR);
    }
}

void CWorkPage::OnBnClickedStop() {
    std::wstring error;
    bool ok = services::JobManager::Instance().StopSession(error);
    size_t count = services::JobManager::Instance().GetStepCount();
    std::wstring zipPath = services::JobManager::Instance().GetActiveZipPath();

    if (ok) {
        CString msg;
        msg.Format(L"Successfully saved %zu steps to:\n%s\n\nWould you like to open the destination folder?",
                   count, zipPath.c_str());
        if (AfxMessageBox(msg, MB_YESNO | MB_ICONINFORMATION) == IDYES) {
            platform::ProcessRunner::OpenFolderAndSelectFile(zipPath);
        }
    } else {
        AfxMessageBox((L"Failed to save recording:\n" + error).c_str(), MB_ICONERROR);
    }
}

void CWorkPage::OnDropFiles(HDROP hDropInfo) {
    wchar_t filePath[MAX_PATH] = { 0 };
    if (DragQueryFileW(hDropInfo, 0, filePath, MAX_PATH)) {
        m_editPath.SetWindowTextW(filePath);
    }
    DragFinish(hDropInfo);
}

void CWorkPage::OnSize(UINT nType, int cx, int cy) {
    CDialogEx::OnSize(nType, cx, cy);

    if (!m_listSteps.GetSafeHwnd()) return;

    // Dynamically resize steps list and status to fill available client area
    CRect rectClient;
    GetClientRect(&rectClient);

    CRect listRect;
    m_listSteps.GetWindowRect(&listRect);
    ScreenToClient(&listRect);

    int newHeight = rectClient.bottom - listRect.top - 28;
    int newWidth = rectClient.right - listRect.left - 10;
    if (newHeight > 50 && newWidth > 100) {
        m_listSteps.SetWindowPos(nullptr, 0, 0, newWidth, newHeight, SWP_NOMOVE | SWP_NOZORDER);

        CRect statusRect;
        m_staticStatus.GetWindowRect(&statusRect);
        ScreenToClient(&statusRect);
        m_staticStatus.SetWindowPos(nullptr, listRect.left, rectClient.bottom - 22, newWidth, 18, SWP_NOZORDER);
    }
}

void CWorkPage::AddStepToUI(const core::Step& step) {
    if (!m_listSteps.GetSafeHwnd()) return;

    // Check if updating existing typing step
    for (int i = 0; i < m_listSteps.GetItemCount(); ++i) {
        if (m_listSteps.GetItemData(i) == static_cast<DWORD_PTR>(step.n)) {
            m_listSteps.SetItemText(i, 3, step.description.c_str());
            return;
        }
    }

    int idx = m_listSteps.GetItemCount();
    int inserted = m_listSteps.InsertItem(idx, std::to_wstring(step.n).c_str());
    m_listSteps.SetItemText(inserted, 1, step.time.c_str());
    m_listSteps.SetItemText(inserted, 2, step.kind.c_str());
    m_listSteps.SetItemText(inserted, 3, step.description.c_str());
    m_listSteps.SetItemText(inserted, 4, step.window.c_str());
    m_listSteps.SetItemData(inserted, static_cast<DWORD_PTR>(step.n));

    m_listSteps.EnsureVisible(inserted, FALSE);

    std::wstring countStr = L"Steps recorded: " + std::to_wstring(m_listSteps.GetItemCount());
    m_staticCount.SetWindowTextW(countStr.c_str());
}

void CWorkPage::UpdateStatusUI(const std::wstring& status) {
    if (m_staticStatus.GetSafeHwnd()) {
        m_staticStatus.SetWindowTextW(status.c_str());
    }
}

void CWorkPage::UpdateStateUI(core::JobStatus state) {
    if (!m_btnStart.GetSafeHwnd()) return;

    bool isRecording = (state == core::JobStatus::Recording);
    bool isFinalizing = (state == core::JobStatus::Finalizing);
    const bool optionsEnabled = !isRecording && !isFinalizing;

    // Dialog navigation must never start from a disabled control. In
    // particular, a real Start/Stop click leaves focus on that button.
    const HWND focused = ::GetFocus();
    const auto willDisableFocus = [focused](const CWnd& control, bool enabled) {
        return !enabled && focused && focused == control.GetSafeHwnd();
    };
    if (willDisableFocus(m_btnStart, optionsEnabled) ||
        willDisableFocus(m_btnStop, isRecording) ||
        willDisableFocus(m_editPath, optionsEnabled) ||
        willDisableFocus(m_btnBrowse, optionsEnabled) ||
        willDisableFocus(m_chkClicks, optionsEnabled) ||
        willDisableFocus(m_chkKeys, optionsEnabled) ||
        willDisableFocus(m_chkHighlight, optionsEnabled) ||
        willDisableFocus(m_chkFullScreen, optionsEnabled)) {
        CWnd* nextFocus = isRecording ? static_cast<CWnd*>(&m_btnStop) :
            isFinalizing ? static_cast<CWnd*>(&m_listSteps) : static_cast<CWnd*>(&m_btnStart);
        nextFocus->EnableWindow(TRUE);
        GetParent()->SendMessage(WM_NEXTDLGCTL,
            reinterpret_cast<WPARAM>(nextFocus->GetSafeHwnd()), TRUE);
    }

    m_btnStart.EnableWindow(optionsEnabled);
    m_btnStop.EnableWindow(isRecording);

    m_editPath.EnableWindow(optionsEnabled);
    m_btnBrowse.EnableWindow(optionsEnabled);
    m_chkClicks.EnableWindow(optionsEnabled);
    m_chkKeys.EnableWindow(optionsEnabled);
    m_chkHighlight.EnableWindow(optionsEnabled);
    m_chkFullScreen.EnableWindow(optionsEnabled);

    if (isRecording) {
        m_progress.SetMarquee(TRUE, 30);
    } else if (isFinalizing) {
        m_progress.SetMarquee(TRUE, 15);
    } else {
        m_progress.SetMarquee(FALSE, 0);
        m_progress.SetPos(0);
    }
}

} // namespace steprec::app
