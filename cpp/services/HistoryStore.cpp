#include "HistoryStore.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>

namespace steprec::services {

namespace {

std::wstring UnescapeJsonString(const std::string& str) {
    std::wstring out;
    out.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            char next = str[i + 1];
            switch (next) {
            case '"': out += L'"'; ++i; break;
            case '\\': out += L'\\'; ++i; break;
            case 'n': out += L'\n'; ++i; break;
            case 'r': out += L'\r'; ++i; break;
            case 't': out += L'\t'; ++i; break;
            default: out += static_cast<wchar_t>(next); ++i; break;
            }
        } else {
            out += static_cast<wchar_t>(static_cast<unsigned char>(str[i]));
        }
    }
    return out;
}

std::string ExtractJsonFieldValue(const std::string& line, const std::string& key) {
    std::string pattern = "\"" + key + "\":";
    size_t pos = line.find(pattern);
    if (pos == std::string::npos) return {};

    pos += pattern.length();
    while (pos < line.length() && (line[pos] == ' ' || line[pos] == '\t')) {
        ++pos;
    }
    if (pos >= line.length()) return {};

    if (line[pos] == '"') {
        // String value
        size_t start = pos + 1;
        size_t end = start;
        bool inEscape = false;
        while (end < line.length()) {
            if (line[end] == '\\' && !inEscape) {
                inEscape = true;
            } else if (line[end] == '"' && !inEscape) {
                break;
            } else {
                inEscape = false;
            }
            ++end;
        }
        return line.substr(start, end - start);
    } else {
        // Number or boolean
        size_t start = pos;
        size_t end = start;
        while (end < line.length() && line[end] != ',' && line[end] != '}' && line[end] != '\r' && line[end] != '\n') {
            ++end;
        }
        return line.substr(start, end - start);
    }
}

} // anonymous namespace

HistoryStore& HistoryStore::Instance() {
    static HistoryStore instance;
    return instance;
}

HistoryStore::HistoryStore() {
    Load();
}

std::wstring HistoryStore::GetAppDataDirectory() const {
    PWSTR pPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &pPath))) {
        std::wstring dir = std::wstring(pPath) + L"\\StepRecorder";
        CoTaskMemFree(pPath);
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    wchar_t tempBuf[MAX_PATH];
    GetTempPathW(MAX_PATH, tempBuf);
    return std::wstring(tempBuf) + L"StepRecorder";
}

std::wstring HistoryStore::GetHistoryFilePath() const {
    return GetAppDataDirectory() + L"\\history.json";
}

bool HistoryStore::Load() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_records.clear();
    m_loaded = true;

    std::wstring filePath = GetHistoryFilePath();
    std::ifstream file(filePath);
    if (!file) {
        return false;
    }

    std::string line;
    core::JobMetadata current;
    bool inObject = false;

    while (std::getline(file, line)) {
        if (line.find('{') != std::string::npos) {
            current = core::JobMetadata{};
            inObject = true;
        }

        if (inObject) {
            std::string id = ExtractJsonFieldValue(line, "id");
            if (!id.empty()) current.id = UnescapeJsonString(id);

            std::string ts = ExtractJsonFieldValue(line, "timestamp");
            if (!ts.empty()) current.timestamp = UnescapeJsonString(ts);

            std::string count = ExtractJsonFieldValue(line, "stepCount");
            if (!count.empty()) current.stepCount = std::atoi(count.c_str());

            std::string duration = ExtractJsonFieldValue(line, "durationSeconds");
            if (!duration.empty()) current.durationSeconds = std::atoi(duration.c_str());

            std::string status = ExtractJsonFieldValue(line, "status");
            if (!status.empty()) current.status = UnescapeJsonString(status);

            std::string path = ExtractJsonFieldValue(line, "targetZipPath");
            if (!path.empty()) current.targetZipPath = UnescapeJsonString(path);

            std::string err = ExtractJsonFieldValue(line, "errorMessage");
            if (!err.empty()) current.errorMessage = UnescapeJsonString(err);
        }

        if (line.find('}') != std::string::npos && inObject) {
            if (!current.id.empty()) {
                m_records.push_back(current);
            }
            inObject = false;
        }
    }

    return true;
}

bool HistoryStore::Save() {
    std::wstring appDir = GetAppDataDirectory();
    std::wstring mainPath = appDir + L"\\history.json";
    std::wstring tempPath = appDir + L"\\history.json.tmp";

    std::ofstream file(tempPath, std::ios::trunc);
    if (!file) return false;

    file << "[\n";
    for (size_t i = 0; i < m_records.size(); ++i) {
        const auto& r = m_records[i];
        file << "  {\n";
        file << "    \"id\": " << core::EscapeJsonString(r.id) << ",\n";
        file << "    \"timestamp\": " << core::EscapeJsonString(r.timestamp) << ",\n";
        file << "    \"stepCount\": " << r.stepCount << ",\n";
        file << "    \"durationSeconds\": " << r.durationSeconds << ",\n";
        file << "    \"status\": " << core::EscapeJsonString(r.status) << ",\n";
        file << "    \"targetZipPath\": " << core::EscapeJsonString(r.targetZipPath) << ",\n";
        file << "    \"errorMessage\": " << core::EscapeJsonString(r.errorMessage) << "\n";
        file << "  }";
        if (i + 1 < m_records.size()) {
            file << ",";
        }
        file << "\n";
    }
    file << "]\n";
    file.close();

    if (!file) {
        DeleteFileW(tempPath.c_str());
        return false;
    }

    return MoveFileExW(tempPath.c_str(), mainPath.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

void HistoryStore::AddRecord(const core::JobMetadata& record) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_records.insert(m_records.begin(), record);
    }
    Save();
}

bool HistoryStore::UpdateRecord(const core::JobMetadata& record) {
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& r : m_records) {
            if (r.id == record.id) {
                r = record;
                found = true;
                break;
            }
        }
    }
    if (found) {
        Save();
    }
    return found;
}

bool HistoryStore::RemoveRecord(const std::wstring& id) {
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_records.begin(); it != m_records.end(); ++it) {
            if (it->id == id) {
                m_records.erase(it);
                removed = true;
                break;
            }
        }
    }
    if (removed) {
        Save();
    }
    return removed;
}

void HistoryStore::ClearAll() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_records.clear();
    }
    Save();
}

std::vector<core::JobMetadata> HistoryStore::GetRecords() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_records;
}

void HistoryStore::ReconcileOnStartup() {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& r : m_records) {
            if (r.status == L"Recording" || r.status == L"Saving ZIP...") {
                r.status = L"Interrupted";
                r.errorMessage = L"Application closed unexpectedly during recording.";
                changed = true;
            }
        }
    }
    if (changed) {
        Save();
    }
}

} // namespace steprec::services
