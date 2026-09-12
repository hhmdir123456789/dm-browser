#pragma once
#include <map>
#include <mutex>
#include <string>
#include "common/Types.h"
#include "common/Error.h"
#include "common/Result.h"

enum class MarketTier { Official, Community, Unsigned };

static inline std::string tierName(MarketTier t) {
    switch (t) {
        case MarketTier::Official:  return "official";
        case MarketTier::Community: return "community";
        case MarketTier::Unsigned:  return "unsigned";
    }
    return "unknown";
}

class SignatureVerifier {
public:
    void addKey(const std::string& keyId, const std::string& pub,
                MarketTier tier) {
        std::lock_guard lock(mu_);
        keys_[keyId] = KeyInfo{pub, tier, 0};
    }

    Result<MarketTier> verify(const std::string& keyId,
                              const std::string& payload,
                              const std::string& sig) const {
        std::lock_guard lock(mu_);
        auto it = keys_.find(keyId);
        if (it == keys_.end())
            return Result<MarketTier>::fail(
                Error::auth(ErrCode::KeyNotFound, "key not found"));
        if (it->second.expiresAt > 0 && nowMs() > it->second.expiresAt)
            return Result<MarketTier>::fail(
                Error::auth(ErrCode::SignatureExpired, "key expired"));
        if (sig != "sig:" + keyId + ":" + payload)
            return Result<MarketTier>::fail(
                Error::auth(ErrCode::SignatureInvalid, "signature mismatch"));
        return Result<MarketTier>::ok(it->second.maxTier);
    }

    static std::string makeSignature(const std::string& keyId,
                                     const std::string& payload) {
        return "sig:" + keyId + ":" + payload;
    }

private:
    struct KeyInfo { std::string pub; MarketTier maxTier; int64_t expiresAt{0}; };
    mutable std::mutex mu_;
    std::map<std::string, KeyInfo> keys_;
};
