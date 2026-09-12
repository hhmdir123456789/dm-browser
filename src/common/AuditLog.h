#pragma once
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>
#include "common/Types.h"
#include "db/Database.h"

struct AuditEntry {
    int64_t seq{0};
    TraceId traceId;
    PluginId pluginId;
    std::string endpoint;
    std::string method;
    std::string result;
    std::string errorCode;
    int64_t durationMs{0};
    int64_t createdAt{0};
};

class AuditLog {
public:
    explicit AuditLog(Database& db) : db_(db) {}

    void append(const AuditEntry& e) {
        std::lock_guard lock(mu_);
        db_.exec(
            "INSERT INTO audit_log (trace_id, plugin_id, endpoint, method, "
            "result, error_code, duration_ms, created_at) VALUES (?,?,?,?,?,?,?,?)",
            {e.traceId, e.pluginId, e.endpoint, e.method,
             e.result, e.errorCode, std::to_string(e.durationMs),
             std::to_string(e.createdAt)});
    }

    std::vector<AuditEntry> recent(size_t n) const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT seq, trace_id, plugin_id, endpoint, method, result, "
            "error_code, duration_ms, created_at FROM audit_log "
            "ORDER BY seq DESC LIMIT ?",
            {std::to_string(n)});
        std::vector<AuditEntry> out;
        for (auto& r : rows) out.push_back(rowToEntry(r));
        std::reverse(out.begin(), out.end());
        return out;
    }

    std::vector<AuditEntry> byPlugin(const PluginId& pid) const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT seq, trace_id, plugin_id, endpoint, method, result, "
            "error_code, duration_ms, created_at FROM audit_log "
            "WHERE plugin_id = ? ORDER BY seq",
            {pid});
        std::vector<AuditEntry> out;
        for (auto& r : rows) out.push_back(rowToEntry(r));
        return out;
    }

    std::vector<AuditEntry> all() const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT seq, trace_id, plugin_id, endpoint, method, result, "
            "error_code, duration_ms, created_at FROM audit_log ORDER BY seq");
        std::vector<AuditEntry> out;
        for (auto& r : rows) out.push_back(rowToEntry(r));
        return out;
    }

    size_t size() const {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT COUNT(*) FROM audit_log");
        return rows.empty() ? 0 : std::stoul(rows[0][0]);
    }

private:
    static AuditEntry rowToEntry(const std::vector<std::string>& r) {
        AuditEntry e;
        e.seq = std::stoll(r[0]);
        e.traceId = r[1];
        e.pluginId = r[2];
        e.endpoint = r[3];
        e.method = r[4];
        e.result = r[5];
        e.errorCode = r[6];
        e.durationMs = std::stoll(r[7]);
        e.createdAt = std::stoll(r[8]);
        return e;
    }

    mutable std::mutex mu_;
    Database& db_;
};
