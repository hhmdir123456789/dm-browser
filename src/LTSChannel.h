#pragma once
#include <mutex>
#include <string>
#include "common/Types.h"
#include "common/Result.h"
#include "db/Database.h"

enum class ReleaseChannel { Stable, LTS, Patch };
static inline std::string channelName(ReleaseChannel c) {
    switch (c) {
        case ReleaseChannel::Stable: return "stable";
        case ReleaseChannel::LTS:    return "lts";
        case ReleaseChannel::Patch:  return "patch";
    }
    return "unknown";
}

struct ReleaseInfo {
    std::string version;
    ReleaseChannel channel{ReleaseChannel::Stable};
    int64_t publishedAt{0};
    int64_t eolAt{0};
};

class LTSChannel {
public:
    explicit LTSChannel(Database& db) : db_(db) {}

    void addRelease(const ReleaseInfo& r) {
        std::lock_guard lock(mu_);
        db_.exec(
            "INSERT OR REPLACE INTO release_record (version, channel, published_at, eol_at) "
            "VALUES (?,?,?,?)",
            {r.version, channelName(r.channel), std::to_string(r.publishedAt),
             std::to_string(r.eolAt)});
    }

    std::string currentLTS() const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT version FROM release_record WHERE channel = 'lts' "
            "ORDER BY published_at DESC LIMIT 1");
        return rows.empty() ? "" : rows[0][0];
    }

    bool isEOL(const std::string& version) const {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT eol_at FROM release_record WHERE version = ?",
                              {version});
        if (rows.empty()) return true;
        int64_t eol = std::stoll(rows[0][0]);
        if (eol == 0) return false;
        return nowMs() > eol;
    }

    size_t count() const {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT COUNT(*) FROM release_record");
        return rows.empty() ? 0 : std::stoul(rows[0][0]);
    }

private:
    mutable std::mutex mu_;
    Database& db_;
};
