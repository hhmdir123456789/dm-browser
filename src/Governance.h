#pragma once
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "common/Types.h"
#include "common/Result.h"
#include "db/Database.h"

enum class GovernanceRole {
    CoreMaintainer, SecurityReviewer, EcosystemSteward, CommunityContributor
};

static inline std::string roleName(GovernanceRole r) {
    switch (r) {
        case GovernanceRole::CoreMaintainer:      return "core-maintainer";
        case GovernanceRole::SecurityReviewer:    return "security-reviewer";
        case GovernanceRole::EcosystemSteward:    return "ecosystem-steward";
        case GovernanceRole::CommunityContributor:return "community-contributor";
    }
    return "unknown";
}

struct Governor {
    std::string memberId;
    std::string displayName;
    GovernanceRole role{GovernanceRole::CommunityContributor};
    int64_t joinedAt{0};
};

struct Decision {
    std::string decisionId;
    std::string title;
    std::string rationale;
    std::string finalRuling;
    int64_t decidedAt{0};
};

class GovernanceCouncil {
public:
    explicit GovernanceCouncil(Database& db) : db_(db) {}

    bool addMember(const Governor& g) {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT member_id FROM governor WHERE member_id = ?",
                              {g.memberId});
        if (!rows.empty()) return false;
        db_.exec(
            "INSERT INTO governor (member_id, display_name, role, joined_at) "
            "VALUES (?,?,?,?)",
            {g.memberId, g.displayName, roleName(g.role), std::to_string(nowMs())});
        return true;
    }

    bool removeMember(const std::string& id) {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT member_id FROM governor WHERE member_id = ?", {id});
        if (rows.empty()) return false;
        db_.exec("DELETE FROM governor WHERE member_id = ?", {id});
        return true;
    }

    std::vector<Governor> listMembers() const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT member_id, display_name, role, joined_at FROM governor ORDER BY member_id");
        std::vector<Governor> out;
        for (auto& r : rows)
            out.push_back({r[0], r[1], roleFromName(r[2]), std::stoll(r[3])});
        return out;
    }

    void recordDecision(const Decision& d) {
        std::lock_guard lock(mu_);
        db_.exec(
            "INSERT OR REPLACE INTO decision (decision_id, title, rationale, "
            "final_ruling, decided_at) VALUES (?,?,?,?,?)",
            {d.decisionId, d.title, d.rationale, d.finalRuling,
             std::to_string(nowMs())});
    }

    Result<bool> vote(const std::string& decisionId, const std::string& memberId,
                      bool inFavor) {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT member_id FROM governor WHERE member_id = ?",
                              {memberId});
        if (rows.empty())
            return Result<bool>::fail(
                Error::auth(ErrCode::GrantNotFound, "not a member"));
        db_.exec(
            "INSERT OR REPLACE INTO vote (decision_id, member_id, in_favor, created_at) "
            "VALUES (?,?,?,?)",
            {decisionId, memberId, inFavor ? "1" : "0", std::to_string(nowMs())});
        return Result<bool>::ok(true);
    }

    std::pair<size_t, size_t> tally(const std::string& decisionId) const {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT in_favor, COUNT(*) FROM vote WHERE decision_id = ? GROUP BY in_favor",
            {decisionId});
        size_t f = 0, a = 0;
        for (auto& r : rows) {
            if (r[0] == "1") f = std::stoul(r[1]);
            else a = std::stoul(r[1]);
        }
        return {f, a};
    }

    size_t memberCount() const {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT COUNT(*) FROM governor");
        return rows.empty() ? 0 : std::stoul(rows[0][0]);
    }

    size_t decisionCount() const {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT COUNT(*) FROM decision");
        return rows.empty() ? 0 : std::stoul(rows[0][0]);
    }

private:
    static GovernanceRole roleFromName(const std::string& n) {
        if (n == "core-maintainer") return GovernanceRole::CoreMaintainer;
        if (n == "security-reviewer") return GovernanceRole::SecurityReviewer;
        if (n == "ecosystem-steward") return GovernanceRole::EcosystemSteward;
        return GovernanceRole::CommunityContributor;
    }

    mutable std::mutex mu_;
    Database& db_;
};
