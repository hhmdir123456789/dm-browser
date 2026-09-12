#include "test_framework.h"
#include "db/Database.h"

using namespace dm::test;

TEST(Persist_BasicInsertQuery) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)");
    db.exec("INSERT INTO t (name) VALUES (?)", {"alice"});
    auto rows = db.query("SELECT name FROM t");
    EXPECT_EQ(rows.size(), (size_t)1);
    EXPECT_EQ(rows[0][0], std::string("alice"));
}

TEST(Persist_CountRows) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY)");
    db.exec("INSERT INTO t DEFAULT VALUES");
    db.exec("INSERT INTO t DEFAULT VALUES");
    auto rows = db.query("SELECT COUNT(*) FROM t");
    EXPECT_EQ(rows[0][0], std::string("2"));
}
