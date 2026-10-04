#include "pch.h"
#include "SettingsDialog.h"

namespace steprec::app {

BEGIN_MESSAGE_MAP(CSettingsDialog, CDialogEx)
END_MESSAGE_MAP()

CSettingsDialog::CSettingsDialog(CWnd* pParent)
    : CDialogEx(IDD_SETTINGS_DIALOG, pParent) {}

void CSettingsDialog::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_CLICK_DELAY, m_editClickDelay);
    DDX_Control(pDX, IDC_EDIT_KEY_DELAY, m_editKeyDelay);
    DDX_Control(pDX, IDC_EDIT_TYPING_GAP, m_editTypingGap);
}

BOOL CSettingsDialog::OnInitDialog() {
    CDialogEx::OnInitDialog();

    SetDlgItemInt(IDC_EDIT_CLICK_DELAY, m_clickDelayMs, FALSE);
    SetDlgItemInt(IDC_EDIT_KEY_DELAY, m_keyDelayMs, FALSE);
    SetDlgItemInt(IDC_EDIT_TYPING_GAP, m_typingGapMs, FALSE);

    return TRUE;
}

void CSettingsDialog::OnOK() {
    BOOL translated = FALSE;
    UINT clickVal = GetDlgItemInt(IDC_EDIT_CLICK_DELAY, &translated, FALSE);
    if (translated && clickVal >= 50 && clickVal <= 5000) {
        m_clickDelayMs = static_cast<int>(clickVal);
    }

    UINT keyVal = GetDlgItemInt(IDC_EDIT_KEY_DELAY, &translated, FALSE);
    if (translated && keyVal >= 50 && keyVal <= 5000) {
        m_keyDelayMs = static_cast<int>(keyVal);
    }

    UINT typingVal = GetDlgItemInt(IDC_EDIT_TYPING_GAP, &translated, FALSE);
    if (translated && typingVal >= 100 && typingVal <= 10000) {
        m_typingGapMs = static_cast<int>(typingVal);
    }

    CDialogEx::OnOK();
}

} // namespace steprec::app
