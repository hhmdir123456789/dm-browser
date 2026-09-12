#include "test_framework.h"
#include "db/Database.h"
#include "SecureStore.h"

using namespace dm::test;

TEST(M3_SecureStoreSaveLookup) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE IF NOT EXISTS credentials (origin TEXT, username TEXT, encrypted_password TEXT, created_at INTEGER, PRIMARY KEY (origin, username))");
    SecureStore store(db);
    EXPECT_TRUE(store.store("https://bank.dm", "alice", "pw"));
    auto creds = store.lookup("https://bank.dm");
    EXPECT_EQ(creds.size(), (size_t)1);
    EXPECT_EQ(creds[0].username, std::string("alice"));
}
