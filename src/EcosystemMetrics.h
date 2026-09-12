#pragma once
#include <mutex>
#include "Market.h"
#include "Dispute.h"
#include "Governance.h"

struct EcosystemMetricsData {
    size_t pluginTotal{0};
    size_t officialCount{0};
    size_t communityCount{0};
    size_t unsignedCount{0};
    size_t withdrawnCount{0};
    double avgEndpoints{0.0};
    double avgScore{0.0};
    size_t governorCount{0};
    size_t decisionCount{0};
    size_t disputeTotal{0};
    size_t disputeOpen{0};
    size_t disputeResolved{0};
    size_t disputeAppealed{0};
    double avgResolutionDays{0.0};
    double healthScore{0.0};
};

class EcosystemMetrics {
public:
    EcosystemMetrics(PluginMarket& m, DisputeManager& d, GovernanceCouncil& c)
        : market_(m), disputes_(d), council_(c) {}

    EcosystemMetricsData compute() const {
        std::lock_guard lock(mu_);
        EcosystemMetricsData m;
        auto plugins = market_.list();
        m.pluginTotal = plugins.size();
        size_t epSum = 0;
        double scoreSum = 0.0;
        for (const auto& p : plugins) {
            if (p.tier == MarketTier::Official) m.officialCount++;
            else if (p.tier == MarketTier::Community) m.communityCount++;
            else m.unsignedCount++;
            if (p.withdrawnAt > 0) m.withdrawnCount++;
            epSum += p.endpoints.size();
            scoreSum += p.score;
        }
        if (m.pluginTotal > 0) {
            m.avgEndpoints = static_cast<double>(epSum) / m.pluginTotal;
            m.avgScore = scoreSum / m.pluginTotal;
        }
        m.governorCount = council_.memberCount();
        m.decisionCount = council_.decisionCount();
        m.disputeTotal = disputes_.count();
        for (const auto& c : disputes_.all()) {
            switch (c.status) {
                case DisputeStatus::Open:
                case DisputeStatus::Investigating: m.disputeOpen++; break;
                case DisputeStatus::Resolved: m.disputeResolved++; break;
                case DisputeStatus::Appealed: m.disputeAppealed++; break;
                default: break;
            }
        }
        m.avgResolutionDays = disputes_.avgResolutionMs() / 86400000.0;
        m.healthScore = computeHealth(m);
        return m;
    }

private:
    static double computeHealth(const EcosystemMetricsData& m) {
        double s = 100.0;
        if (m.pluginTotal > 0) {
            double signedRatio = static_cast<double>(m.officialCount + m.communityCount)
                                 / m.pluginTotal;
            if (signedRatio < 0.8) s -= (0.8 - signedRatio) * 50;
            double wr = static_cast<double>(m.withdrawnCount) / m.pluginTotal;
            if (wr > 0.01) s -= (wr - 0.01) * 200;
        }
        if (m.avgEndpoints > 8) s -= (m.avgEndpoints - 8) * 2;
        if (m.disputeOpen > 5) s -= (m.disputeOpen - 5) * 1.5;
        if (m.governorCount < 4) s -= (4 - m.governorCount) * 3;
        if (s < 0) s = 0;
        if (s > 100) s = 100;
        return s;
    }

    mutable std::mutex mu_;
    PluginMarket& market_;
    DisputeManager& disputes_;
    GovernanceCouncil& council_;
};
