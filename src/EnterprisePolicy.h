#pragma once
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include "common/Types.h"
#include "common/Result.h"
#include "db/Database.h"

struct EnterprisePolicyData {
    std::string policyId;
    std::set<PluginId> pluginWhitelist;
    std::map<std::string, int64_t> endpointQuota;
    int64_t auditRetentionDays{7};
    std::string mirrorUrl;
};

class EnterprisePolicy {
public:
    explicit EnterprisePolicy(Database& db) : db_(db) {}

    Result<bool> load(const EnterprisePolicyData& d,
                      const std::string& adminKeyId,
                      const std::string& adminSig) {
        std::lock_guard lock(mu_);
        std::string payload = d.policyId + ":" + d.mirrorUrl;
        if (adminSig != "admin-sig:" + adminKeyId + ":" + payload)
            return Result<bool>::fail(
                Error::auth(ErrCode::SignatureInvalid, "admin sig invalid"));
        db_.exec(
            "INSERT OR REPLACE INTO enterprise_policy "
            "(policy_id, admin_signature, plugin_whitelist, endpoint_quota, "
            "audit_retention_days, mirror_url, updated_at) VALUES (?,?,?,?,?,?,?)",
            {d.policyId, adminSig, joinSet(d.pluginWhitelist),
             joinQuota(d.endpointQuota), std::to_string(d.auditRetentionDays),
             d.mirrorUrl, std::to_string(nowMs())});
        loaded_ = true;
        return Result<bool>::ok(true);
    }

    bool isLoaded() const { std::lock_guard lock(mu_); return loaded_; }

    bool isWhitelisted(const PluginId& pid) const {
        std::lock_guard lock(mu_);
        if (!loaded_) return true;
        auto rows = db_.query("SELECT plugin_whitelist FROM enterprise_policy LIMIT 1");
        if (rows.empty()) return true;
        auto wl = split(rows[0][0]);
        if (wl.empty()) return true;
        for (const auto& w : wl) if (w == pid) return true;
        return false;
    }

    bool checkQuota(const std::string& ep) {
        std::lock_guard lock(mu_);
        if (!loaded_) return true;
        auto rows = db_.query("SELECT endpoint_quota FROM enterprise_policy LIMIT 1");
        if (rows.empty()) return true;
        auto quotas = parseQuota(rows[0][0]);
        auto it = quotas.find(ep);
        if (it == quotas.end()) return true;
        int64_t now = nowMs();
        auto& w = windows_[ep];
        if (now - w.start > 3600000) { w.start = now; w.count = 0; }
        if (w.count >= it->second) return false;
        w.count++;
        return true;
    }

    static std::string makeAdminSignature(const std::string& keyId,
                                          const EnterprisePolicyData& d) {
        return "admin-sig:" + keyId + ":" + d.policyId + ":" + d.mirrorUrl;
    }

private:
    struct Window { int64_t start{0}; int64_t count{0}; };

    static std::string joinSet(const std::set<PluginId>& s) {
        std::string out = "[";
        bool first = true;
        for (const auto& v : s) {
            if (!first) out += ",";
            out += "\"" + v + "\"";
            first = false;
        }
        out += "]";
        return out;
    }
    static std::string joinQuota(const std::map<std::string, int64_t>& m) {
        std::string out = "{";
        bool first = true;
        for (const auto& [k, v] : m) {
            if (!first) out += ",";
            out += "\"" + k + "\":" + std::to_string(v);
            first = false;
        }
        out += "}";
        return out;
    }
    static std::vector<std::string> split(const std::string& s) {
        std::vector<std::string> out;
        std::string cur;
        bool inStr = false;
        for (char c : s) {
            if (c == '"') { inStr = !inStr; continue; }
            if (c == ',' && !inStr) { if (!cur.empty()) out.push_back(cur); cur.clear(); continue; }
            if (c == '[' || c == ']') continue;
            cur += c;
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    }
    static std::map<std::string, int64_t> parseQuota(const std::string& s) {
        std::map<std::string, int64_t> out;
        std::string cur;
        bool inStr = false;
        std::string key;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '"') { inStr = !inStr; continue; }
            if (c == ':' && !inStr) { key = cur; cur.clear(); continue; }
            if ((c == ',' || c == '}') && !inStr) {
                if (!key.empty() && !cur.empty()) out[key] = std::stoll(cur);
                key.clear(); cur.clear(); continue;
            }
            if (c == '{' || c == ' ') continue;
            cur += c;
        }
        return out;
    }

    mutable std::mutex mu_;
    Database& db_;
    bool loaded_{false};
    std::map<std::string, Window> windows_;
};