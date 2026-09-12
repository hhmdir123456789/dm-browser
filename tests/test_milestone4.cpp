#include "test_framework.h"
#include "Signature.h"

using namespace dm::test;

TEST(M4_SignatureVerify) {
    SignatureVerifier v;
    v.addKey("k1", "pub", MarketTier::Official);
    auto r = v.verify("k1", "payload", "sig:k1:payload");
    EXPECT_TRUE(r.ok());
    EXPECT_EQ((int)r.value(), (int)MarketTier::Official);
}

TEST(M4_SignatureInvalid) {
    SignatureVerifier v;
    v.addKey("k1", "pub", MarketTier::Official);
    auto r = v.verify("k1", "payload", "wrong");
    EXPECT_TRUE(!r.ok());
}
