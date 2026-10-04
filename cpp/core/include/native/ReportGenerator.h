#pragma once

#include "native/Types.h"
#include <vector>
#include <string>

namespace steprec::core {

class ReportGenerator {
public:
    static std::wstring BuildHtml(const std::vector<Step>& steps, const std::wstring& sessionTitle = L"Step Recording");
    static std::string BuildJson(const std::vector<Step>& steps);
};

} // namespace steprec::core
