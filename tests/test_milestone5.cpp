#include "test_framework.h"
#include "EnterprisePolicy.h"
#include "db/Database.h"

using namespace dm::test;

TEST(M5_PolicyLoad) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE IF NOT EXISTS enterprise_policy (policy_id TEXT PRIMARY KEY, admin_signature TEXT, plugin_whitelist TEXT, endpoint_quota TEXT, audit_retention_days INTEGER, mirror_url TEXT, updated_at INTEGER)");
    EnterprisePolicy p(db);
    EnterprisePolicyData d;
    d.policyId = "ent-1";
    d.mirrorUrl = "https://mirror.dm";
    std::string sig = EnterprisePolicy::makeAdminSignature("admin", d);
    auto r = p.load(d, "admin", sig);
    EXPECT_TRUE(r.ok());
    EXPECT_TRUE(p.isLoaded());
}
