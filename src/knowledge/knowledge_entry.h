#pragma once
#include <string>
#include <vector>

namespace dm::knowledge {

struct KnowledgeEntry {
    std::string id;
    std::string category;        // html / css / js
    std::string name;
    std::string specUrl;
    std::string summary;
    std::string syntax;
    std::vector<std::string> values;
    std::vector<std::string> examples;

    bool implemented = false;
    std::string implementedAt;
    std::vector<std::string> relatedCode;
    int difficulty = 1;
    std::vector<std::string> dependencies;
    std::vector<std::string> relatedFeatures;

    // 从学习库统计（运行时填充）
    int failCount = 0;
    int passCount = 0;
    int priority = 0;

    bool isMissing() const { return !implemented; }

    double failRate() const {
        int total = failCount + passCount;
        return total > 0 ? (double)failCount / total : 0.0;
    }
};

} // namespace dm::knowledge