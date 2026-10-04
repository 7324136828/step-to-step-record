#pragma once

#include <string>

namespace steprec::services {

class ArchiveService {
public:
    static bool ExportArchive(const std::wstring& sourceZip,
                             const std::wstring& destinationZip,
                             std::wstring& errorMessage);

    static bool ExtractReportToTemp(const std::wstring& zipPath,
                                   std::wstring& outHtmlPath,
                                   std::wstring& errorMessage);
};

} // namespace steprec::services
