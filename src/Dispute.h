#pragma once
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "common/Types.h"
#include "common/Result.h"
#include "db/Database.h"

enum class DisputeStatus { Open, Investigating, Resolved, Appealed, Closed };
enum class DisputeRuling { None, Dismissed, Warning, Suspended, Withdrawn, Restored };

static inline std::string disputeStatusName(DisputeStatus s) {
    switch (s) {
        case DisputeStatus::Open:          return "open";
        case DisputeStatus::Investigating: return "investigating";
        case DisputeStatus::Resolved:      return "resolved";
        case DisputeStatus::Appealed:      return "appealed";
        case DisputeStatus::Closed:        return "closed";
    }
    return "unknown";
}
static inline std::string rulingName(DisputeRuling r) {
    switch (r) {
        case DisputeRuling::None:      return "none";
        case DisputeRuling::Dismissed: return "dismissed";
        case DisputeRuling::Warning:   return "warning";
        case DisputeRuling::Suspended: return "suspended";
        case DisputeRuling::Withdrawn: return "withdrawn";
        case DisputeRuling::Restored:  return "restored";
    }
    return "unknown";
}

struct DisputeCase {
    CaseId caseId;
    PluginId pluginId;
    std::string reporterId;
    std::string reason;
    DisputeStatus status{DisputeStatus::Open};
    DisputeRuling ruling{DisputeRuling::None};
    std::string resolution;
    int64_t createdAt{0};
    int64_t resolvedAt{0};
    int64_t closedAt{0};
};

class DisputeManager {
public:
    explicit DisputeManager(Database& db) : db_(db) {}

    Result<CaseId> report(const PluginId& pid, const std::string& reporter,
                          const std::string& reason) {
        std::lock_guard lock(mu_);
        if (pid.empty() || reporter.empty() || reason.empty())
            return Result<CaseId>::fail(
                Error::scope(ErrCode::ManifestInvalid, "invalid report"));
        CaseId id = "case-" + std::to_string(nowMs()) + "-" + std::to_string(nextId_++);
        db_.exec(
            "INSERT INTO dispute_case (case_id, plugin_id, reporter_id, reason, "
            "status, created_at) VALUES (?,?,?,?,?,?)",
            {id, pid, reporter, reason, "open", std::to_string(nowMs())});
        return Result<CaseId>::ok(id);
    }

    Result<bool> investigate(const CaseId& id) {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT status FROM dispute_case WHERE case_id = ?", {id});
        if (rows.empty())
            return Result<bool>::fail(
                Error::scope(ErrCode::CaseNotFound, "case not found"));
        if (rows[0][0] != "open")
            return Result<bool>::fail(
                Error::scope(ErrCode::CaseAlreadyClosed, "case not open"));
        db_.exec("UPDATE dispute_case SET status = 'investigating' WHERE case_id = ?", {id});
        return Result<bool>::ok(true);
    }

    Result<bool> resolve(const CaseId& id, DisputeRuling ruling,
                         const std::string& resolution) {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT status FROM dispute_case WHERE case_id = ?", {id});
        if (rows.empty())
            return Result<bool>::fail(
                Error::scope(ErrCode::CaseNotFound, "case not found"));
        if (rows[0][0] == "closed")
            return Result<bool>::fail(
                Error::scope(ErrCode::CaseAlreadyClosed, "case already closed"));
        db_.exec(
            "UPDATE dispute_case SET status = 'resolved', ruling = ?, "
            "resolution = ?, resolved_at = ? WHERE case_id = ?",
            {rulingName(ruling), resolution, std::to_string(nowMs()), id});
        return Result<bool>::ok(true);
    }

    Result<bool> appeal(const CaseId& id, const std::string& note) {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT status FROM dispute_case WHERE case_id = ?", {id});
        if (rows.empty())
            return Result<bool>::fail(
                Error::scope(ErrCode::CaseNotFound, "case not found"));
        if (rows[0][0] != "resolved")
            return Result<bool>::fail(
                Error::scope(ErrCode::CaseAlreadyClosed, "case not resolved"));
        db_.exec("UPDATE dispute_case SET status = 'appealed' WHERE case_id = ?", {id});
        db_.exec("INSERT INTO dispute_appeal (case_id, note, created_at) VALUES (?,?,?)",
                 {id, note, std::to_string(nowMs())});
        return Result<bool>::ok(true);
    }

    Result<bool> close(const CaseId& id) {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT case_id FROM dispute_case WHERE case_id = ?", {id});
        if (rows.empty())
            return Result<bool>::fail(
                Error::scope(ErrCode::CaseNotFound, "case not found"));
        db_.exec("UPDATE dispute_case SET status = 'closed', closed_at = ? WHERE case_id = ?",
                 {std::to_string(nowMs()), id});
        return Result<bool>::ok(true);
    }

    std::optional<DisputeCase> get(const CaseId& id) const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT case_id, plugin_id, reporter_id, reason, status, ruling, "
            "resolution, created_at, resolved_at, closed_at "
            "FROM dispute_case WHERE case_id = ?", {id});
        if (rows.empty()) return std::nullopt;
        return rowToCase(rows[0]);
    }

    std::vector<DisputeCase> all() const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT case_id, plugin_id, reporter_id, reason, status, ruling, "
            "resolution, created_at, resolved_at, closed_at "
            "FROM dispute_case ORDER BY case_id");
        std::vector<DisputeCase> out;
        for (auto& r : rows) out.push_back(rowToCase(r));
        return out;
    }

    size_t count() const {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT COUNT(*) FROM dispute_case");
        return rows.empty() ? 0 : std::stoul(rows[0][0]);
    }

    double avgResolutionMs() const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT AVG(resolved_at - created_at) FROM dispute_case "
            "WHERE resolved_at > 0");
        if (rows.empty() || rows[0][0].empty()) return 0.0;
        return std::stod(rows[0][0]);
    }

private:
    static DisputeCase rowToCase(const std::vector<std::string>& r) {
        DisputeCase c;
        c.caseId = r[0]; c.pluginId = r[1]; c.reporterId = r[2]; c.reason = r[3];
        c.status = statusFromName(r[4]); c.ruling = rulingFromName(r[5]);
        c.resolution = r[6]; c.createdAt = std::stoll(r[7]);
        c.resolvedAt = std::stoll(r[8]); c.closedAt = std::stoll(r[9]);
        return c;
    }
    static DisputeStatus statusFromName(const std::string& n) {
        if (n == "open") return DisputeStatus::Open;
        if (n == "investigating") return DisputeStatus::Investigating;
        if (n == "resolved") return DisputeStatus::Resolved;
        if (n == "appealed") return DisputeStatus::Appealed;
        return DisputeStatus::Closed;
    }
    static DisputeRuling rulingFromName(const std::string& n) {
        if (n == "dismissed") return DisputeRuling::Dismissed;
        if (n == "warning") return DisputeRuling::Warning;
        if (n == "suspended") return DisputeRuling::Suspended;
        if (n == "withdrawn") return DisputeRuling::Withdrawn;
        if (n == "restored") return DisputeRuling::Restored;
        return DisputeRuling::None;
    }

    mutable std::mutex mu_;
    Database& db_;
    int32_t nextId_{1};
};
