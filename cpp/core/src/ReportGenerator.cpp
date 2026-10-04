#include "native/ReportGenerator.h"

namespace steprec::core {

std::wstring ReportGenerator::BuildHtml(const std::vector<Step>& steps, const std::wstring& sessionTitle) {
    std::wstring rows;
    rows.reserve(steps.size() * 512);

    for (const auto& s : steps) {
        rows += L"<tr>";
        rows += L"<td class=\"num\">" + std::to_wstring(s.n) + L"</td>";
        rows += L"<td class=\"time\">" + EscapeHtml(s.time) + L"</td>";
        rows += L"<td class=\"kind\"><span class=\"badge badge-" + EscapeHtml(s.kind) + L"\">" + EscapeHtml(s.kind) + L"</span></td>";
        rows += L"<td class=\"action\"><strong>" + EscapeHtml(s.description) + L"</strong></td>";
        rows += L"<td class=\"window\"><div class=\"win-title\">" + EscapeHtml(s.window) + L"</div>";
        if (!s.process.empty()) {
            rows += L"<div class=\"win-proc\">" + EscapeHtml(s.process) + L"</div>";
        }
        rows += L"</td>";
        rows += L"<td class=\"shot\">";
        if (!s.image.empty()) {
            rows += L"<a href=\"" + EscapeHtml(s.image) + L"\" target=\"_blank\">";
            rows += L"<img src=\"" + EscapeHtml(s.image) + L"\" alt=\"Step " + std::to_wstring(s.n) + L" Screenshot\" loading=\"lazy\">";
            rows += L"</a>";
        }
        rows += L"</td>";
        rows += L"</tr>\n";
    }

    std::wstring html =
        L"<!doctype html>\n"
        L"<html lang=\"en\">\n"
        L"<head>\n"
        L"  <meta charset=\"utf-8\">\n"
        L"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        L"  <title>" + EscapeHtml(sessionTitle) + L"</title>\n"
        L"  <style>\n"
        L"    :root {\n"
        L"      --bg: #f8fafc;\n"
        L"      --card-bg: #ffffff;\n"
        L"      --text: #1e293b;\n"
        L"      --muted: #64748b;\n"
        L"      --border: #e2e8f0;\n"
        L"      --primary: #0284c7;\n"
        L"      --primary-light: #e0f2fe;\n"
        L"    }\n"
        L"    body {\n"
        L"      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;\n"
        L"      background-color: var(--bg);\n"
        L"      color: var(--text);\n"
        L"      margin: 0;\n"
        L"      padding: 32px 24px;\n"
        L"      line-height: 1.5;\n"
        L"    }\n"
        L"    .container { max-width: 1280px; margin: 0 auto; }\n"
        L"    header {\n"
        L"      background: var(--card-bg);\n"
        L"      border: 1px solid var(--border);\n"
        L"      border-radius: 8px;\n"
        L"      padding: 24px 32px;\n"
        L"      margin-bottom: 24px;\n"
        L"      box-shadow: 0 1px 3px rgba(0,0,0,0.05);\n"
        L"    }\n"
        L"    h1 { margin: 0 0 8px 0; font-size: 24px; font-weight: 600; color: #0f172a; }\n"
        L"    .meta { font-size: 14px; color: var(--muted); }\n"
        L"    table {\n"
        L"      width: 100%;\n"
        L"      border-collapse: collapse;\n"
        L"      background: var(--card-bg);\n"
        L"      border: 1px solid var(--border);\n"
        L"      border-radius: 8px;\n"
        L"      overflow: hidden;\n"
        L"      box-shadow: 0 1px 3px rgba(0,0,0,0.05);\n"
        L"    }\n"
        L"    th, td {\n"
        L"      padding: 12px 16px;\n"
        L"      text-align: left;\n"
        L"      vertical-align: top;\n"
        L"      border-bottom: 1px solid var(--border);\n"
        L"    }\n"
        L"    th {\n"
        L"      background-color: #f1f5f9;\n"
        L"      font-size: 13px;\n"
        L"      font-weight: 600;\n"
        L"      text-transform: uppercase;\n"
        L"      letter-spacing: 0.05em;\n"
        L"      color: #475569;\n"
        L"    }\n"
        L"    tr:last-child td { border-bottom: none; }\n"
        L"    tr:hover td { background-color: #f8fafc; }\n"
        L"    .num { width: 40px; font-weight: 600; color: var(--muted); text-align: center; }\n"
        L"    .time { width: 90px; font-size: 13px; color: var(--muted); white-space: nowrap; }\n"
        L"    .kind { width: 80px; }\n"
        L"    .badge {\n"
        L"      display: inline-block;\n"
        L"      padding: 3px 8px;\n"
        L"      font-size: 11px;\n"
        L"      font-weight: 600;\n"
        L"      border-radius: 9999px;\n"
        L"      text-transform: uppercase;\n"
        L"      letter-spacing: 0.05em;\n"
        L"    }\n"
        L"    .badge-click { background: #dbeafe; color: #1e40af; }\n"
        L"    .badge-type  { background: #dcfce7; color: #166534; }\n"
        L"    .badge-key   { background: #fef3c7; color: #92400e; }\n"
        L"    .action { font-size: 14px; min-width: 200px; }\n"
        L"    .win-title { font-weight: 500; font-size: 13px; color: #334155; }\n"
        L"    .win-proc { font-size: 12px; color: var(--muted); font-style: italic; margin-top: 2px; }\n"
        L"    .shot { width: 340px; }\n"
        L"    .shot img {\n"
        L"      max-width: 320px;\n"
        L"      max-height: 200px;\n"
        L"      border-radius: 4px;\n"
        L"      border: 1px solid var(--border);\n"
        L"      box-shadow: 0 1px 2px rgba(0,0,0,0.05);\n"
        L"      transition: transform 0.15s ease, box-shadow 0.15s ease;\n"
        L"      display: block;\n"
        L"    }\n"
        L"    .shot img:hover {\n"
        L"      transform: scale(1.02);\n"
        L"      box-shadow: 0 4px 8px rgba(0,0,0,0.12);\n"
        L"    }\n"
        L"  </style>\n"
        L"</head>\n"
        L"<body>\n"
        L"  <div class=\"container\">\n"
        L"    <header>\n"
        L"      <h1>" + EscapeHtml(sessionTitle) + L"</h1>\n"
        L"      <div class=\"meta\">Recorded Steps: " + std::to_wstring(steps.size()) + L"</div>\n"
        L"    </header>\n"
        L"    <table>\n"
        L"      <thead>\n"
        L"        <tr>\n"
        L"          <th>#</th>\n"
        L"          <th>Time</th>\n"
        L"          <th>Type</th>\n"
        L"          <th>Action Details</th>\n"
        L"          <th>Window / Process</th>\n"
        L"          <th>Screenshot</th>\n"
        L"        </tr>\n"
        L"      </thead>\n"
        L"      <tbody>\n" + rows +
        L"      </tbody>\n"
        L"    </table>\n"
        L"  </div>\n"
        L"</body>\n"
        L"</html>\n";

    return html;
}

std::string ReportGenerator::BuildJson(const std::vector<Step>& steps) {
    std::string out = "[\n";
    for (size_t i = 0; i < steps.size(); ++i) {
        const auto& s = steps[i];
        out += "  {\n";
        out += "    \"n\": " + std::to_string(s.n) + ",\n";
        out += "    \"time\": " + EscapeJsonString(s.time) + ",\n";
        out += "    \"kind\": " + EscapeJsonString(s.kind) + ",\n";
        out += "    \"description\": " + EscapeJsonString(s.description) + ",\n";
        out += "    \"window\": " + EscapeJsonString(s.window) + ",\n";
        out += "    \"process\": " + EscapeJsonString(s.process) + ",\n";
        out += "    \"image\": " + EscapeJsonString(s.image) + "\n";
        out += "  }";
        if (i + 1 < steps.size()) {
            out += ",";
        }
        out += "\n";
    }
    out += "]\n";
    return out;
}

} // namespace steprec::core
