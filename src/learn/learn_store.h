#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
#include "db/Database.h"

namespace dm::learn {

struct SiteSample {
    int64_t id = 0;
    std::string url;
    std::string title;
    std::string html;
    std::string css;
    int htmlSize = 0;
    int64_t capturedAt = 0;
    std::string refScreenshot;
    std::string dmScreenshot;
    double diffScore = 0.0;
    std::string status = "pending";
};

struct DiffRecord {
    int64_t id = 0;
    int64_t sampleId = 0;
    std::string category;
    int severity = 1;
    std::string feature;
    std::string description;
    std::string aiAnalysis;
    int bboxX = 0, bboxY = 0, bboxW = 0, bboxH = 0;
    int64_t createdAt = 0;
};

struct FeatureStat {
    std::string feature;
    int tested = 0;
    int passed = 0;
    int failed = 0;
    int priority = 0;
};

struct RefInfo {
    int64_t id = 0;
    std::string url;
    std::string title;
    int64_t capturedAt = 0;
    std::string snapshotJson;
};

class LearnStore {
public:
    explicit LearnStore(Database& db) : db_(db) {}

    bool init();

    // ---------- 网站样本 ----------
    int64_t addSample(const SiteSample& s);
    bool updateSampleScreenshots(int64_t id, const std::string& ref,
                                 const std::string& dm, double diff);
    bool updateSampleStatus(int64_t id, const std::string& status);
    SiteSample getSample(int64_t id);
    SiteSample getSampleByUrl(const std::string& url);
    std::vector<SiteSample> listSamples(int limit = 100);
    std::vector<SiteSample> listByStatus(const std::string& status);

    // ---------- 差异记录 ----------
    int64_t addDiff(const DiffRecord& d);
    std::vector<DiffRecord> getDiffs(int64_t sampleId);
    std::vector<DiffRecord> listRecentDiffs(int limit = 50);

    // ---------- 特性覆盖率 ----------
    void recordFeature(const std::string& feature, bool passed);
    std::vector<FeatureStat> listFeaturesByPriority();
    void recalcPriorities();

    // ---------- 快照 JSON 存取（批 4B-2） ----------
    bool saveSnapshot(const std::string& url, const std::string& snapshotJson);
    std::string loadSnapshot(const std::string& url);

    // ---------- 参考快照查询（批 C） ----------
    std::vector<RefInfo> listRefs(int limit = 100);
    RefInfo getRef(const std::string& url);

    // ---------- 统计 ----------
    int sampleCount();
    int diffCount();
    int fixedCount();

private:
    Database& db_;
    std::mutex mu_;
};

} // namespace dm::learn