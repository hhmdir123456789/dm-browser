#pragma once
#include <sstream>
#include "EcosystemMetrics.h"

class AnnualReport {
public:
    explicit AnnualReport(EcosystemMetrics& m) : metrics_(m) {}

    std::string generate(int year) const {
        auto m = metrics_.compute();
        std::ostringstream oss;
        oss << "==== 大明DM浏览器 生态年度报告 " << year << " ====\n\n";
        oss << "一、插件生态\n";
        oss << "  总数: " << m.pluginTotal << "\n";
        oss << "  官方/社区/未签名: " << m.officialCount << "/"
            << m.communityCount << "/" << m.unsignedCount << "\n";
        oss << "  已撤回: " << m.withdrawnCount << "\n";
        oss << "  平均端点: " << m.avgEndpoints << "\n\n";
        oss << "二、治理\n";
        oss << "  成员: " << m.governorCount << ", 决策: " << m.decisionCount << "\n\n";
        oss << "三、争议\n";
        oss << "  总数: " << m.disputeTotal << ", 进行中: " << m.disputeOpen
            << ", 已裁决: " << m.disputeResolved
            << ", 已申诉: " << m.disputeAppealed << "\n";
        oss << "  平均处理: " << m.avgResolutionDays << " 天\n\n";
        oss << "四、健康度\n";
        oss << "  评分: " << m.healthScore << " / 100\n";
        return oss.str();
    }

    std::string summary() const {
        auto m = metrics_.compute();
        std::ostringstream oss;
        oss << "plugins=" << m.pluginTotal
            << " official=" << m.officialCount
            << " community=" << m.communityCount
            << " unsigned=" << m.unsignedCount
            << " disputes=" << m.disputeTotal
            << " health=" << m.healthScore;
        return oss.str();
    }

private:
    EcosystemMetrics& metrics_;
};
