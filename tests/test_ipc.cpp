#include "test_framework.h"
#include "ipc/Framing.h"

using namespace dm::test;

TEST(IPC_EncodeDecodeInvoke) {
    auto frame = dm::ipc::Framing::encodeInvoke(
        "g-1", "Ping", "hello", "args", "t-1", "https://a.dm");
    auto kind = dm::ipc::Framing::peekKind(frame);
    EXPECT_EQ((int)kind, (int)dm::ipc::Framing::Kind::Invoke);
    auto m = dm::ipc::Framing::decodeInvoke(frame);
    EXPECT_TRUE(m.ok());
    EXPECT_EQ(m.value().endpoint, std::string("Ping"));
    EXPECT_EQ(m.value().method, std::string("hello"));
}

TEST(IPC_EncodeDecodeResult) {
    auto frame = dm::ipc::Framing::encodeResult(true, "value", 0, "");
    auto kind = dm::ipc::Framing::peekKind(frame);
    EXPECT_EQ((int)kind, (int)dm::ipc::Framing::Kind::Result);
    auto m = dm::ipc::Framing::decodeResult(frame);
    EXPECT_TRUE(m.ok());
    EXPECT_TRUE(m.value().ok);
    EXPECT_EQ(m.value().value, std::string("value"));
}
