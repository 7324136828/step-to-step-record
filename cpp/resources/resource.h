#pragma once

#define IDR_MAINFRAME                   128
#define IDD_MAIN_DIALOG                 102
#define IDD_WORK_PAGE                   103
#define IDD_HISTORY_PAGE                104
#define IDD_SETTINGS_DIALOG             105

// Main Dialog Controls
#define IDC_TAB_MAIN                    1000
#define IDC_BTN_SETTINGS                1001

// Work Page Controls
#define IDC_EDIT_PATH                   1010
#define IDC_BTN_BROWSE                  1011
#define IDC_CHK_CLICKS                  1012
#define IDC_CHK_KEYS                    1013
#define IDC_CHK_HIGHLIGHT               1014
#define IDC_CHK_FULLSCREEN              1015
#define IDC_BTN_START                   1016
#define IDC_BTN_STOP                    1017
#define IDC_STATIC_COUNT                1018
#define IDC_STATIC_STATUS               1019
#define IDC_PROGRESS_RECORDING          1020
#define IDC_LIST_STEPS                  1021

// History Page Controls
#define IDC_LIST_HISTORY                1030
#define IDC_BTN_OPEN_ZIP                1031
#define IDC_BTN_OPEN_FOLDER             1032
#define IDC_BTN_VIEW_REPORT             1033
#define IDC_BTN_DELETE_RECORD           1034
#define IDC_BTN_CLEAR_HISTORY           1035
#define IDC_BTN_REFRESH                 1036

// Settings Dialog Controls
#define IDC_EDIT_CLICK_DELAY            1040
#define IDC_EDIT_KEY_DELAY              1041
#define IDC_EDIT_TYPING_GAP             1042

// Custom application messages
#define WM_APP_STEP_RECORDED            (WM_APP + 100)
#define WM_APP_STATUS_CHANGED           (WM_APP + 101)
#define WM_APP_STATE_CHANGED            (WM_APP + 102)
