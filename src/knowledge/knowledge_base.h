#pragma once
#include "knowledge/knowledge_entry.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace dm::knowledge {

class KnowledgeBase {
public:
    KnowledgeBase() = default;

    // 加载知识库目录（含 html.json / css.json / js.json）
    bool load(const std::string& dir);

    // 按 id 精确查找
    const KnowledgeEntry* findById(const std::string& id) const;

    // 按 feature 名（学习库里用的）查找
    const KnowledgeEntry* findByFeature(const std::string& feature) const;

    // 按分类列出
    std::vector<const KnowledgeEntry*> byCategory(const std::string& cat) const;

    // 所有未实现的，按优先级排序
    std::vector<const KnowledgeEntry*> notImplemented() const;

    // 搜索
    std::vector<const KnowledgeEntry*> search(const std::string& keyword) const;

    // 和运行时失败统计合并
    void updateStats(const std::map<std::string, std::pair<int,int>>& stats);

    // 生成"下一步该做什么"报告
    std::string generatePriorityReport() const;

    // 生成 AI 修复 Prompt
    std::string buildFixPrompt(
        const std::string& entryId,
        const std::string& codeSnippet,
        const std::string& failureContext) const;

    size_t total() const;
    size_t implementedCount() const;
    size_t missingCount() const;

private:
    std::vector<KnowledgeEntry> entries_;
    std::map<std::string, size_t> byId_;
    std::map<std::string, size_t> byFeature_;
    mutable std::mutex mu_;
};

} // namespace dm::knowledge