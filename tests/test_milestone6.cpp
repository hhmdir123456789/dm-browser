#include "test_framework.h"
#include "db/Database.h"
#include "Dispute.h"

using namespace dm::test;

TEST(M6_DisputeReport) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE IF NOT EXISTS dispute_case (case_id TEXT PRIMARY KEY, plugin_id TEXT, reporter_id TEXT, reason TEXT, status TEXT, ruling TEXT, resolution TEXT, created_at INTEGER, resolved_at INTEGER, closed_at INTEGER)");
    db.exec("CREATE TABLE IF NOT EXISTS dispute_appeal (case_id TEXT, note TEXT, created_at INTEGER)");
    DisputeManager dm(db);
    auto r = dm.report("plugin.a", "user1", "reason");
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(dm.count(), (size_t)1);
}
