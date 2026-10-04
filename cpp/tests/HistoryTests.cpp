#include "TestHarness.h"
#include "HistoryStore.h"
#include "native/Types.h"
#include <iostream>
#include <windows.h>

TEST_CASE(History_AddUpdateRemovePersistence) {
    auto& store = steprec::services::HistoryStore::Instance();
    std::wstring testId = L"test_job_123456";

    // Clean up if already exists
    store.RemoveRecord(testId);

    steprec::core::JobMetadata rec;
    rec.id = testId;
    rec.timestamp = L"2026-10-03 12:00:00";
    rec.stepCount = 42;
    rec.durationSeconds = 15;
    rec.status = L"Completed";
    rec.targetZipPath = L"C:\\Temp\\test_output.zip";
    rec.errorMessage = L"";

    store.AddRecord(rec);

    // Verify record exists
    auto records = store.GetRecords();
    bool found = false;
    for (const auto& r : records) {
        if (r.id == testId) {
            ASSERT_EQ(r.stepCount, 42);
            ASSERT_EQ(r.durationSeconds, 15);
            ASSERT_EQ(r.status, L"Completed");
            ASSERT_EQ(r.targetZipPath, L"C:\\Temp\\test_output.zip");
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);

    // Test update
    rec.status = L"Failed";
    rec.errorMessage = L"Test error description";
    bool updated = store.UpdateRecord(rec);
    ASSERT_TRUE(updated);

    records = store.GetRecords();
    for (const auto& r : records) {
        if (r.id == testId) {
            ASSERT_EQ(r.status, L"Failed");
            ASSERT_EQ(r.errorMessage, L"Test error description");
            break;
        }
    }

    // Test remove
    bool removed = store.RemoveRecord(testId);
    ASSERT_TRUE(removed);

    records = store.GetRecords();
    for (const auto& r : records) {
        ASSERT_NE(r.id, testId);
    }

    return true;
}

TEST_CASE(History_StartupReconciliation) {
    auto& store = steprec::services::HistoryStore::Instance();
    std::wstring recId = L"test_job_unreconciled";

    store.RemoveRecord(recId);

    steprec::core::JobMetadata rec;
    rec.id = recId;
    rec.timestamp = L"2026-10-03 12:00:00";
    rec.status = L"Recording";
    rec.targetZipPath = L"C:\\Temp\\abandoned.zip";
    store.AddRecord(rec);

    // Reconcile on startup
    store.ReconcileOnStartup();

    auto records = store.GetRecords();
    bool checked = false;
    for (const auto& r : records) {
        if (r.id == recId) {
            ASSERT_EQ(r.status, L"Interrupted");
            ASSERT_FALSE(r.errorMessage.empty());
            checked = true;
            break;
        }
    }
    ASSERT_TRUE(checked);

    store.RemoveRecord(recId);
    return true;
}
