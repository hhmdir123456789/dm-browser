#pragma once
#include <mutex>
#include <string>
#include "common/Types.h"
#include "common/Error.h"
#include "common/Result.h"
#include "db/Database.h"

class GrantStore {
public:
    explicit GrantStore(Database& db) : db_(db) {}

    GrantId grant(const PluginId& pid, const std::string& ep,
                  const std::string& scope = "{}", bool confirm = false) {
        std::lock_guard lock(mu_);
        GrantId id = "g-" + std::to_string(nowMs()) + "-" + std::to_string(nextId_++);
        db_.exec(
            "INSERT INTO capability_grant (grant_id, plugin_id, endpoint, scope, "
            "version, granted_at, expires_at, revoked_at, requires_confirmation, signature) "
            "VALUES (?,?,?,?,?,?,?,?,?,?)",
            {id, pid, ep, scope, "1", std::to_string(nowMs()),
             "0", "0", confirm ? "1" : "0", ""});
        return id;
    }

    bool validate(const GrantId& id, const std::string& ep) const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT endpoint, COALESCE(revoked_at, 0) FROM capability_grant "
            "WHERE grant_id = ?",
            {id});
        if (rows.empty()) return false;
        try {
            if (std::stoll(rows[0][1]) > 0) return false;
        } catch (...) {
            return false;
        }
        return rows[0][0] == ep;
    }

    void revokeAll(const PluginId& pid) {
        std::lock_guard lock(mu_);
        db_.exec("UPDATE capability_grant SET revoked_at = ? WHERE plugin_id = ?",
                 {std::to_string(nowMs()), pid});
    }

    size_t size() const {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT COUNT(*) FROM capability_grant");
        return rows.empty() ? 0 : std::stoul(rows[0][0]);
    }

private:
    mutable std::mutex mu_;
    Database& db_;
    int32_t nextId_{1};
};