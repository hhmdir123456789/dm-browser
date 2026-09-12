#pragma once
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "common/Types.h"
#include "common/Result.h"
#include "Signature.h"
#include "db/Database.h"

struct PluginManifest {
    PluginId pluginId;
    std::string name;
    std::string version;
    std::vector<std::string> endpoints;
    MarketTier tier{MarketTier::Unsigned};
    std::string authorId;
};

class PluginHost {
public:
    explicit PluginHost(Database& db) : db_(db) {}

    bool load(const PluginManifest& m) {
        std::lock_guard lock(mu_);
        auto existing = db_.query("SELECT plugin_id FROM plugin_manifest WHERE plugin_id = ?",
                                  {m.pluginId});
        if (!existing.empty()) return false;
        db_.exec(
            "INSERT INTO plugin_manifest (plugin_id, name, version, tier, "
            "endpoints, author_id, cached_at) VALUES (?,?,?,?,?,?,?)",
            {m.pluginId, m.name, m.version, tierName(m.tier),
             join(m.endpoints), m.authorId, std::to_string(nowMs())});
        db_.exec("INSERT INTO plugin_state (plugin_id, state) VALUES (?,?)",
                 {m.pluginId, "installed"});
        return true;
    }

    bool enable(const PluginId& id)   { return transition(id, "installed", "enabled"); }
    bool activate(const PluginId& id) { return transition(id, "enabled", "active"); }
    bool suspend(const PluginId& id)  { return transition(id, "active", "suspended"); }

    bool unload(const PluginId& id) {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT state FROM plugin_state WHERE plugin_id = ?", {id});
        if (rows.empty() || rows[0][0] == "unloaded") return false;
        db_.exec("UPDATE plugin_state SET state = 'unloaded', unloaded_at = ? WHERE plugin_id = ?",
                 {std::to_string(nowMs()), id});
        return true;
    }

    std::optional<PluginManifest> manifest(const PluginId& id) const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT plugin_id, name, version, tier, endpoints, author_id "
            "FROM plugin_manifest WHERE plugin_id = ?", {id});
        if (rows.empty()) return std::nullopt;
        PluginManifest m;
        m.pluginId = rows[0][0];
        m.name = rows[0][1];
        m.version = rows[0][2];
        m.tier = tierFromName(rows[0][3]);
        m.endpoints = split(rows[0][4]);
        m.authorId = rows[0][5];
        return m;
    }

    std::optional<std::string> state(const PluginId& id) const {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT state FROM plugin_state WHERE plugin_id = ?", {id});
        if (rows.empty()) return std::nullopt;
        return rows[0][0];
    }

private:
    bool transition(const PluginId& id, const std::string& from, const std::string& to) {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT state FROM plugin_state WHERE plugin_id = ?", {id});
        if (rows.empty() || rows[0][0] != from) return false;
        db_.exec("UPDATE plugin_state SET state = ? WHERE plugin_id = ?", {to, id});
        return true;
    }

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

    mutable std::mutex mu_;
    Database& db_;
};
