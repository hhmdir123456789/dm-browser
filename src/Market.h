#pragma once
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "common/Types.h"
#include "common/Result.h"
#include "Signature.h"
#include "db/Database.h"

struct MarketPlugin {
    PluginId pluginId;
    std::string name;
    std::string version;
    MarketTier tier{MarketTier::Unsigned};
    std::string keyId;
    std::string signature;
    std::vector<std::string> endpoints;
    std::vector<std::string> privacyLabels;
    double score{0.0};
    int64_t downloads{0};
    std::string authorId;
    int64_t publishedAt{0};
    int64_t withdrawnAt{0};
};

class PluginMarket {
public:
    PluginMarket(Database& db, SignatureVerifier& v) : db_(db), verifier_(v) {}

    Result<bool> publish(const MarketPlugin& p, const std::string& payload) {
        std::lock_guard lock(mu_);
        auto existing = db_.query(
            "SELECT withdrawn_at FROM market_plugin WHERE plugin_id = ?", {p.pluginId});
        if (!existing.empty() && std::stoll(existing[0][0]) > 0)
            return Result<bool>::fail(
                Error::auth(ErrCode::PluginWithdrawn, "plugin withdrawn"));

        MarketPlugin e = p;
        if (!p.keyId.empty() && !p.signature.empty()) {
            auto v = verifier_.verify(p.keyId, payload, p.signature);
            if (!v.isOk()) return Result<bool>::fail(v.error());
            e.tier = v.value();
        }
        if (e.tier != MarketTier::Unsigned && e.endpoints.empty())
            return Result<bool>::fail(
                Error::scope(ErrCode::ManifestInvalid, "must declare endpoints"));

        db_.exec(
            "INSERT OR REPLACE INTO market_plugin "
            "(plugin_id, name, version, tier, key_id, signature, endpoints, "
            "privacy_labels, score, downloads, author_id, published_at, withdrawn_at) "
            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
            {e.pluginId, e.name, e.version, tierName(e.tier), e.keyId, e.signature,
             join(e.endpoints), join(e.privacyLabels), std::to_string(e.score),
             std::to_string(e.downloads), e.authorId, std::to_string(nowMs()),
             std::to_string(e.withdrawnAt)});
        return Result<bool>::ok(true);
    }

    bool withdraw(const PluginId& pid) {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT plugin_id FROM market_plugin WHERE plugin_id = ?", {pid});
        if (rows.empty()) return false;
        db_.exec("UPDATE market_plugin SET withdrawn_at = ? WHERE plugin_id = ?",
                 {std::to_string(nowMs()), pid});
        return true;
    }

    std::optional<MarketPlugin> get(const PluginId& pid) const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT plugin_id, name, version, tier, key_id, signature, endpoints, "
            "privacy_labels, score, downloads, author_id, published_at, withdrawn_at "
            "FROM market_plugin WHERE plugin_id = ?", {pid});
        if (rows.empty()) return std::nullopt;
        return rowToPlugin(rows[0]);
    }

    std::vector<MarketPlugin> list() const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT plugin_id, name, version, tier, key_id, signature, endpoints, "
            "privacy_labels, score, downloads, author_id, published_at, withdrawn_at "
            "FROM market_plugin ORDER BY plugin_id");
        std::vector<MarketPlugin> out;
        for (auto& r : rows) out.push_back(rowToPlugin(r));
        return out;
    }

    static bool isSensitive(const std::string& ep) {
        return ep == "SecureStore" || ep == "ScriptInjection" ||
               ep == "TabFullURL" || ep == "CookieStore";
    }

private:
    static std::string join(const std::vector<std::string>& v) {
        std::string out = "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) out += ",";
            out += "\"" + v[i] + "\"";
        }
        out += "]";
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
    static MarketTier tierFromName(const std::string& n) {
        if (n == "official") return MarketTier::Official;
        if (n == "community") return MarketTier::Community;
        return MarketTier::Unsigned;
    }
    static MarketPlugin rowToPlugin(const std::vector<std::string>& r) {
        MarketPlugin p;
        p.pluginId = r[0]; p.name = r[1]; p.version = r[2];
        p.tier = tierFromName(r[3]); p.keyId = r[4]; p.signature = r[5];
        p.endpoints = split(r[6]); p.privacyLabels = split(r[7]);
        p.score = std::stod(r[8]); p.downloads = std::stoll(r[9]);
        p.authorId = r[10]; p.publishedAt = std::stoll(r[11]);
        p.withdrawnAt = std::stoll(r[12]);
        return p;
    }

    mutable std::mutex mu_;
    Database& db_;
    SignatureVerifier& verifier_;
};
