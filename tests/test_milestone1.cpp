#include "test_framework.h"
#include "TabManager.h"

using namespace dm::test;

TEST(M1_CreateTab) {
    TabManager tm;
    TabId id = tm.create("https://start.dm");
    EXPECT_TRUE(id > 0);
    EXPECT_EQ(tm.count(), (size_t)1);
}

TEST(M1_CloseTab) {
    TabManager tm;
    TabId id = tm.create("https://a.dm");
    EXPECT_TRUE(tm.close(id));
    EXPECT_EQ(tm.count(), (size_t)0);
}

TEST(M1_SwitchTab) {
    TabManager tm;
    tm.create("https://a.dm");
    TabId id2 = tm.create("https://b.dm");
    EXPECT_TRUE(tm.activate(id2));
    EXPECT_EQ(tm.activeId(), id2);
}
