#include "test_framework.h"
#include "db/Database.h"
#include "Storage.h"

using namespace dm::test;

TEST(M2_BookmarkAdd) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE IF NOT EXISTS bookmarks (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, url TEXT, created_at INTEGER)");
    BookmarkStore store(db);
    auto r = store.add("DM", "https://dm.dm");
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(store.size(), (size_t)1);
}

TEST(M2_BookmarkRemove) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE IF NOT EXISTS bookmarks (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, url TEXT, created_at INTEGER)");
    BookmarkStore store(db);
    auto r = store.add("A", "https://a.dm");
    EXPECT_TRUE(store.remove(r.value()));
    EXPECT_EQ(store.size(), (size_t)0);
}

TEST(M2_DownloadCreate) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE IF NOT EXISTS downloads (id INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT, filename TEXT, state TEXT, bytes_received INTEGER, total_bytes INTEGER, created_at INTEGER)");
    DownloadStore store(db);
    auto r = store.create("https://f.dm/a.zip", "a.zip");
    EXPECT_TRUE(r.isOk());
    EXPECT_EQ(store.size(), (size_t)1);
}
