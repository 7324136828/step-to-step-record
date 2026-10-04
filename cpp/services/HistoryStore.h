#pragma once

#include "native/Types.h"
#include <string>
#include <vector>
#include <mutex>

namespace steprec::services {

class HistoryStore {
public:
    static HistoryStore& Instance();

    bool Load();
    bool Save();

    void AddRecord(const core::JobMetadata& record);
    bool UpdateRecord(const core::JobMetadata& record);
    bool RemoveRecord(const std::wstring& id);
    void ClearAll();

    std::vector<core::JobMetadata> GetRecords() const;
    void ReconcileOnStartup();

    std::wstring GetHistoryFilePath() const;
    std::wstring GetAppDataDirectory() const;

private:
    HistoryStore();
    ~HistoryStore() = default;

    mutable std::mutex m_mutex;
    std::vector<core::JobMetadata> m_records;
    bool m_loaded{ false };
};

} // namespace steprec::services
