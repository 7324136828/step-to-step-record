# Third-Party Notices

This project is a Windows-native C++ desktop application built using Microsoft Foundation Classes (MFC) and standard Windows Platform APIs.

## Included and Linked System Dependencies

### Microsoft Windows SDK & Microsoft Foundation Classes (MFC)
- **Publisher**: Microsoft Corporation
- **License**: Microsoft Visual Studio / Windows SDK EULA
- **Usage**:
  - Microsoft Foundation Classes (`afxwin.h`, `afxwinappex.h`, `afxcmn.h`, `afxdialogex.h`)
  - GDI+ API (`gdiplus.h`, `gdiplus.lib`) for PNG encoding and rendering
  - Desktop Window Manager API (`dwmapi.h`, `dwmapi.lib`) for window frame cropping
  - Shell API (`shell32.h`, `shlobj.h`) for folder browsing, file drag-and-drop, and known folders
  - Data Protection API (`wincrypt.h`, `crypt32.lib`) for platform credential and data protection

No external non-system third-party packages or interpreters (such as Node.js, Python, or Chromium) are packaged or required at runtime.
