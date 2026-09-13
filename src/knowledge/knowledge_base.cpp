#include "knowledge/knowledge_base.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace dm::knowledge {

using json = nlohmann::json;

static bool loadJsonFile(const std::string& path, json& out) {
    std::ifstream f(path);
    if (!f) return false;
    try {
        out = json::parse(f);
        return true;
    } catch (...) {
        return false;
    }
}

static void parseEntry(const json& j, KnowledgeEntry& e) {
    e.id = j.value("id", "");
    e.category = j.value("category", "");
    e.name = j.value("name", "");
    e.specUrl = j.value("spec_url", "");
    e.summary = j.value("summary", "");
    e.syntax = j.value("syntax", "");
    e.implemented = j.value("implemented", false);
    e.implementedAt = j.value("implemented_at", "");
    e.difficulty = j.value("difficulty", 1);

    if (j.contains("values"))
        for (auto& v : j["values"]) e.values.push_back(v.get<std::string>());
    if (j.contains("examples"))
        for (auto& v : j["examples"]) e.examples.push_back(v.get<std::string>());
    if (j.contains("related_code"))
        for (auto& v : j["related_code"]) e.relatedCode.push_back(v.get<std::string>());
    if (j.contains("dependencies"))
        for (auto& v : j["dependencies"]) e.dependencies.push_back(v.get<std::string>());
    if (j.contains("related_features"))
        for (auto& v : j["related_features"]) e.relatedFeatures.push_back(v.get<std::string>());
}

bool KnowledgeBase::load(const std::string& dir) {
    std::lock_guard<std::mutex> lock(mu_);
    entries_.clear();
    byId_.clear();
    byFeature_.clear();

    const char* files[] = { "html.json", "css.json", "js.json" };
    for (auto* fname : files) {
        std::string path = dir + "/" + fname;
        json j;
        if (!loadJsonFile(path, j)) {
            continue;
        }
        if (!j.contains("entries")) continue;

        for (auto& entryJson : j["entries"]) {
            KnowledgeEntry e;
            parseEntry(entryJson, e);
            if (e.id.empty()) continue;

            size_t idx = entries_.size();
            byId_[e.id] = idx;
            byFeature_[e.id] = idx;
            for (auto& feat : e.relatedFeatures)
                byFeature_[feat] = idx;

            entries_.push_back(std::move(e));
        }
    }
    return !entries_.empty();
}

const KnowledgeEntry* KnowledgeBase::findById(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = byId_.find(id);
    return it != byId_.end() ? &entries_[it->second] : nullptr;
}

const KnowledgeEntry* KnowledgeBase::findByFeature(const std::string& feature) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = byFeature_.find(feature);
    return it != byFeature_.end() ? &entries_[it->second] : nullptr;
}

std::vector<const KnowledgeEntry*> KnowledgeBase::byCategory(
    const std::string& cat) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<const KnowledgeEntry*> out;
    for (auto& e : entries_)
        if (e.category == cat) out.push_back(&e);
    return out;
}

std::vector<const KnowledgeEntry*> KnowledgeBase::notImplemented() const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<const KnowledgeEntry*> out;
    for (auto& e : entries_)
        if (!e.implemented) out.push_back(&e);
    std::sort(out.begin(), out.end(),
        [](const KnowledgeEntry* a, const KnowledgeEntry* b) {
            if (a->failCount != b->failCount) return a->failCount > b->failCount;
            return a->difficulty < b->difficulty;
        });
    return out;
}

std::vector<const KnowledgeEntry*> KnowledgeBase::search(
    const std::string& keyword) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<const KnowledgeEntry*> out;
    std::string k = keyword;
    std::transform(k.begin(), k.end(), k.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    for (auto& e : entries_) {
        std::string n = e.name, s = e.summary;
        std::transform(n.begin(), n.end(), n.begin(),
                       [](unsigned char c){ return (char)std::tolower(c); });
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return (char)std::tolower(c); });
        if (n.find(k) != std::string::npos || s.find(k) != std::string::npos)
            out.push_back(&e);
    }
    return out;
}

void KnowledgeBase::updateStats(
    const std::map<std::string, std::pair<int,int>>& stats) {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& kv : stats) {
        auto it = byFeature_.find(kv.first);
        if (it == byFeature_.end()) continue;
        entries_[it->second].failCount = kv.second.first;
        entries_[it->second].passCount = kv.second.second;
    }
}

size_t KnowledgeBase::total() const {
    std::lock_guard<std::mutex> lock(mu_);
    return entries_.size();
}

size_t KnowledgeBase::implementedCount() const {
    std::lock_guard<std::mutex> lock(mu_);
    size_t n = 0;
    for (auto& e : entries_) if (e.implemented) n++;
    return n;
}

size_t KnowledgeBase::missingCount() const {
    return total() - implementedCount();
}

std::string KnowledgeBase::generatePriorityReport() const {
    std::lock_guard<std::mutex> lock(mu_);
    std::ostringstream os;
    os << "=== 知识库优先级报告 ===\n";
    os << "总计: " << entries_.size() << " 条\n";

    size_t impl = 0;
    for (auto& e : entries_) if (e.implemented) impl++;
    os << "已实现: " << impl << "\n";
    os << "待实现: " << (entries_.size() - impl) << "\n\n";

    os << "【最该实现的 10 个特性】\n";
    std::vector<const KnowledgeEntry*> list;
    for (auto& e : entries_) if (!e.implemented) list.push_back(&e);
    std::sort(list.begin(), list.end(),
        [](const KnowledgeEntry* a, const KnowledgeEntry* b) {
            if (a->failCount != b->failCount) return a->failCount > b->failCount;
            return a->difficulty < b->difficulty;
        });
    int n = 0;
    for (auto* e : list) {
        if (n++ >= 10) break;
        os << "  " << n << ". " << e->id
           << "  失败 " << e->failCount << " 次"
           << "  难度 " << e->difficulty
           << "  — " << e->summary << "\n";
    }
    return os.str();
}

std::string KnowledgeBase::buildFixPrompt(
    const std::string& entryId,
    const std::string& codeSnippet,
    const std::string& failureContext) const {

    auto* e = findById(entryId);
    if (!e) return "";

    std::ostringstream os;
    os << "你是 C++ 渲染引擎开发者。请实现下面的 HTML/CSS/JS 特性。\n\n";
    os << "【特性】\n";
    os << "ID: " << e->id << "\n";
    os << "名称: " << e->name << "\n";
    os << "摘要: " << e->summary << "\n";
    os << "规范: " << e->specUrl << "\n";
    if (!e->syntax.empty()) os << "语法: " << e->syntax << "\n";
    if (!e->values.empty()) {
        os << "可选值: ";
        for (size_t i = 0; i < e->values.size(); i++) {
            if (i) os << ", ";
            os << e->values[i];
        }
        os << "\n";
    }
    os << "\n";

    if (!e->examples.empty()) {
        os << "【正确示例】\n";
        for (auto& ex : e->examples) os << ex << "\n";
        os << "\n";
    }

    os << "【失败背景】\n";
    os << "失败次数: " << e->failCount << "\n";
    if (!failureContext.empty()) os << failureContext << "\n";
    os << "\n";

    os << "【当前实现位置】\n";
    for (auto& f : e->relatedCode) os << "- " << f << "\n";
    os << "\n";

    os << "【相关源码片段】\n";
    os << codeSnippet << "\n\n";

    os << "【要求】\n";
    os << "1. 只修改上述相关文件，不改变公开接口\n";
    os << "2. 保留现有功能，不破坏已通过的测试\n";
    os << "3. 输出 unified diff 格式的 patch\n";
    os << "4. 附带至少 3 个测试用例\n";
    os << "5. 如果实现难度确实很高，输出：SKIP: 原因\n";

    return os.str();
}

} // namespace dm::knowledge