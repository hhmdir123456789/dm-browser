#include "learn/learn_store.h"
#include "common/Types.h"

namespace dm::learn {

bool LearnStore::init() {
    std::lock_guard<std::mutex> lock(mu_);
    const char* stmts[] = {
        "CREATE TABLE IF NOT EXISTS site_sample ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "url TEXT UNIQUE, title TEXT, html TEXT, css TEXT,"
        "html_size INTEGER, captured_at INTEGER,"
        "ref_screenshot TEXT, dm_screenshot TEXT,"
        "diff_score REAL DEFAULT 0, status TEXT DEFAULT 'pending',"
        "snapshot_json TEXT)",

        "CREATE TABLE IF NOT EXISTS diff_record ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "sample_id INTEGER, category TEXT, severity INTEGER,"
        "feature TEXT, description TEXT, ai_analysis TEXT,"
        "bbox_x INTEGER, bbox_y INTEGER, bbox_w INTEGER, bbox_h INTEGER,"
        "created_at INTEGER)",

        "CREATE TABLE IF NOT EXISTS feature_coverage ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "feature TEXT UNIQUE,"
        "tested_count INTEGER DEFAULT 0,"
        "passed_count INTEGER DEFAULT 0,"
        "failed_count INTEGER DEFAULT 0,"
        "priority INTEGER DEFAULT 0,"
        "last_updated INTEGER)",

        "CREATE TABLE IF NOT EXISTS fix_history ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "feature TEXT, commit_hash TEXT,"
        "samples_fixed INTEGER, fixed_at INTEGER)"
    };
    for (auto* s : stmts) {
        auto r = db_.exec(s);
        if (!r.isOk()) return false;
    }
    db_.exec("ALTER TABLE site_sample ADD COLUMN snapshot_json TEXT");
    return true;
}

// ============================================================
// 网站样本
// ============================================================

int64_t LearnStore::addSample(const SiteSample& s) {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query("SELECT id FROM site_sample WHERE url = ?", {s.url});
    if (!rows.empty()) {
        return std::stoll(rows[0][0]);
    }
    db_.exec(
        "INSERT INTO site_sample (url,title,html,css,html_size,captured_at) "
        "VALUES (?,?,?,?,?,?)",
        {s.url, s.title, s.html, s.css, std::to_string(s.htmlSize),
         std::to_string(nowMs())});
    rows = db_.query("SELECT last_insert_rowid()");
    return rows.empty() ? 0 : std::stoll(rows[0][0]);
}

bool LearnStore::updateSampleScreenshots(
    int64_t id, const std::string& ref, const std::string& dm, double diff) {
    std::lock_guard<std::mutex> lock(mu_);
    auto r = db_.exec(
        "UPDATE site_sample SET ref_screenshot=?, dm_screenshot=?, diff_score=? "
        "WHERE id=?",
        {ref, dm, std::to_string(diff), std::to_string(id)});
    return r.isOk();
}

bool LearnStore::updateSampleStatus(int64_t id, const std::string& status) {
    std::lock_guard<std::mutex> lock(mu_);
    auto r = db_.exec("UPDATE site_sample SET status=? WHERE id=?",
                      {status, std::to_string(id)});
    return r.isOk();
}

SiteSample LearnStore::getSample(int64_t id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query(
        "SELECT id,url,title,html,css,html_size,captured_at,"
        "ref_screenshot,dm_screenshot,diff_score,status "
        "FROM site_sample WHERE id=?", {std::to_string(id)});
    SiteSample s;
    if (rows.empty()) return s;
    auto& r = rows[0];
    s.id = std::stoll(r[0]); s.url = r[1]; s.title = r[2];
    s.html = r[3]; s.css = r[4];
    s.htmlSize = std::stoi(r[5]); s.capturedAt = std::stoll(r[6]);
    s.refScreenshot = r[7]; s.dmScreenshot = r[8];
    try { s.diffScore = std::stod(r[9]); } catch (...) {}
    s.status = r[10];
    return s;
}

SiteSample LearnStore::getSampleByUrl(const std::string& url) {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query("SELECT id FROM site_sample WHERE url=?", {url});
    if (rows.empty()) return {};
    int64_t id = std::stoll(rows[0][0]);
    auto full = db_.query(
        "SELECT id,url,title,html,css,html_size,captured_at,"
        "ref_screenshot,dm_screenshot,diff_score,status "
        "FROM site_sample WHERE id=?", {std::to_string(id)});
    SiteSample s;
    if (full.empty()) return s;
    auto& r = full[0];
    s.id = std::stoll(r[0]); s.url = r[1]; s.title = r[2];
    s.html = r[3]; s.css = r[4];
    s.htmlSize = std::stoi(r[5]); s.capturedAt = std::stoll(r[6]);
    s.refScreenshot = r[7]; s.dmScreenshot = r[8];
    try { s.diffScore = std::stod(r[9]); } catch (...) {}
    s.status = r[10];
    return s;
}

std::vector<SiteSample> LearnStore::listSamples(int limit) {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query(
        "SELECT id,url,title,html_size,captured_at,"
        "ref_screenshot,dm_screenshot,diff_score,status "
        "FROM site_sample ORDER BY id DESC LIMIT ?",
        {std::to_string(limit)});
    std::vector<SiteSample> out;
    for (auto& r : rows) {
        SiteSample s;
        s.id = std::stoll(r[0]); s.url = r[1]; s.title = r[2];
        s.htmlSize = std::stoi(r[3]); s.capturedAt = std::stoll(r[4]);
        s.refScreenshot = r[5]; s.dmScreenshot = r[6];
        try { s.diffScore = std::stod(r[7]); } catch (...) {}
        s.status = r[8];
        out.push_back(s);
    }
    return out;
}

std::vector<SiteSample> LearnStore::listByStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query(
        "SELECT id,url,title,html_size,captured_at,"
        "ref_screenshot,dm_screenshot,diff_score,status "
        "FROM site_sample WHERE status=? ORDER BY diff_score DESC",
        {status});
    std::vector<SiteSample> out;
    for (auto& r : rows) {
        SiteSample s;
        s.id = std::stoll(r[0]); s.url = r[1]; s.title = r[2];
        s.htmlSize = std::stoi(r[3]); s.capturedAt = std::stoll(r[4]);
        s.refScreenshot = r[5]; s.dmScreenshot = r[6];
        try { s.diffScore = std::stod(r[7]); } catch (...) {}
        s.status = r[8];
        out.push_back(s);
    }
    return out;
}

// ============================================================
// 差异记录
// ============================================================

int64_t LearnStore::addDiff(const DiffRecord& d) {
    std::lock_guard<std::mutex> lock(mu_);
    db_.exec(
        "INSERT INTO diff_record (sample_id,category,severity,feature,"
        "description,ai_analysis,bbox_x,bbox_y,bbox_w,bbox_h,created_at) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?)",
        {std::to_string(d.sampleId), d.category, std::to_string(d.severity),
         d.feature, d.description, d.aiAnalysis,
         std::to_string(d.bboxX), std::to_string(d.bboxY),
         std::to_string(d.bboxW), std::to_string(d.bboxH),
         std::to_string(nowMs())});
    auto rows = db_.query("SELECT last_insert_rowid()");
    return rows.empty() ? 0 : std::stoll(rows[0][0]);
}

std::vector<DiffRecord> LearnStore::getDiffs(int64_t sampleId) {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query(
        "SELECT id,sample_id,category,severity,feature,description,"
        "ai_analysis,bbox_x,bbox_y,bbox_w,bbox_h,created_at "
        "FROM diff_record WHERE sample_id=? ORDER BY severity DESC",
        {std::to_string(sampleId)});
    std::vector<DiffRecord> out;
    for (auto& r : rows) {
        DiffRecord d;
        d.id = std::stoll(r[0]); d.sampleId = std::stoll(r[1]);
        d.category = r[2]; d.severity = std::stoi(r[3]);
        d.feature = r[4]; d.description = r[5]; d.aiAnalysis = r[6];
        d.bboxX = std::stoi(r[7]); d.bboxY = std::stoi(r[8]);
        d.bboxW = std::stoi(r[9]); d.bboxH = std::stoi(r[10]);
        d.createdAt = std::stoll(r[11]);
        out.push_back(d);
    }
    return out;
}

std::vector<DiffRecord> LearnStore::listRecentDiffs(int limit) {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query(
        "SELECT id,sample_id,category,severity,feature,description,"
        "created_at FROM diff_record ORDER BY id DESC LIMIT ?",
        {std::to_string(limit)});
    std::vector<DiffRecord> out;
    for (auto& r : rows) {
        DiffRecord d;
        d.id = std::stoll(r[0]); d.sampleId = std::stoll(r[1]);
        d.category = r[2]; d.severity = std::stoi(r[3]);
        d.feature = r[4]; d.description = r[5];
        d.createdAt = std::stoll(r[6]);
        out.push_back(d);
    }
    return out;
}

// ============================================================
// 特性覆盖率
// ============================================================

void LearnStore::recordFeature(const std::string& feature, bool passed) {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query("SELECT id FROM feature_coverage WHERE feature=?",
                          {feature});
    if (rows.empty()) {
        db_.exec(
            "INSERT INTO feature_coverage "
            "(feature,tested_count,passed_count,failed_count,last_updated) "
            "VALUES (?,1,?,?,?)",
            {feature, passed ? "1" : "0", passed ? "0" : "1",
             std::to_string(nowMs())});
    } else {
        db_.exec(
            "UPDATE feature_coverage SET "
            "tested_count=tested_count+1,"
            "passed_count=passed_count+?,"
            "failed_count=failed_count+?,"
            "last_updated=? WHERE feature=?",
            {passed ? "1" : "0", passed ? "0" : "1",
             std::to_string(nowMs()), feature});
    }
}

std::vector<FeatureStat> LearnStore::listFeaturesByPriority() {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query(
        "SELECT feature,tested_count,passed_count,failed_count,priority "
        "FROM feature_coverage ORDER BY priority DESC, failed_count DESC");
    std::vector<FeatureStat> out;
    for (auto& r : rows) {
        FeatureStat f;
        f.feature = r[0];
        f.tested = std::stoi(r[1]);
        f.passed = std::stoi(r[2]);
        f.failed = std::stoi(r[3]);
        f.priority = std::stoi(r[4]);
        out.push_back(f);
    }
    return out;
}

void LearnStore::recalcPriorities() {
    std::lock_guard<std::mutex> lock(mu_);
    db_.exec(
        "UPDATE feature_coverage SET priority = "
        "failed_count * 10 + "
        "CAST((tested_count - passed_count) * 100.0 / "
        "MAX(tested_count, 1) AS INTEGER)");
}

// ============================================================
// 快照 JSON 存取（批 4B-2）
// ============================================================

bool LearnStore::saveSnapshot(const std::string& url,
                              const std::string& snapshotJson) {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query("SELECT id FROM site_sample WHERE url = ?", {url});
    if (rows.empty()) {
        auto r = db_.exec(
            "INSERT INTO site_sample "
            "(url, title, snapshot_json, html_size, captured_at) "
            "VALUES (?, ?, ?, 0, ?)",
            {url, url, snapshotJson, std::to_string(nowMs())});
        return r.isOk();
    } else {
        auto r = db_.exec(
            "UPDATE site_sample SET snapshot_json = ? WHERE url = ?",
            {snapshotJson, url});
        return r.isOk();
    }
}

std::string LearnStore::loadSnapshot(const std::string& url) {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query(
        "SELECT snapshot_json FROM site_sample WHERE url = ?", {url});
    if (rows.empty() || rows[0].empty()) return "";
    return rows[0][0];
}

// ============================================================
// 参考快照查询（批 C）
// ============================================================

std::vector<RefInfo> LearnStore::listRefs(int limit) {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query(
        "SELECT id, url, title, captured_at, snapshot_json "
        "FROM site_sample "
        "WHERE snapshot_json IS NOT NULL AND snapshot_json != '' "
        "ORDER BY captured_at DESC LIMIT ?",
        {std::to_string(limit)});
    std::vector<RefInfo> out;
    for (auto& r : rows) {
        RefInfo info;
        info.id = std::stoll(r[0]);
        info.url = r[1];
        info.title = r[2];
        try { info.capturedAt = std::stoll(r[3]); } catch (...) {}
        info.snapshotJson = r[4];
        out.push_back(info);
    }
    return out;
}

RefInfo LearnStore::getRef(const std::string& url) {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query(
        "SELECT id, url, title, captured_at, snapshot_json "
        "FROM site_sample "
        "WHERE url = ? AND snapshot_json IS NOT NULL AND snapshot_json != ''",
        {url});
    RefInfo info;
    if (rows.empty()) return info;
    auto& r = rows[0];
    info.id = std::stoll(r[0]);
    info.url = r[1];
    info.title = r[2];
    try { info.capturedAt = std::stoll(r[3]); } catch (...) {}
    info.snapshotJson = r[4];
    return info;
}

// ============================================================
// 统计
// ============================================================

int LearnStore::sampleCount() {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query("SELECT COUNT(*) FROM site_sample");
    return rows.empty() ? 0 : std::stoi(rows[0][0]);
}

int LearnStore::diffCount() {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query("SELECT COUNT(*) FROM diff_record");
    return rows.empty() ? 0 : std::stoi(rows[0][0]);
}

int LearnStore::fixedCount() {
    std::lock_guard<std::mutex> lock(mu_);
    auto rows = db_.query(
        "SELECT COUNT(*) FROM site_sample WHERE status='fixed'");
    return rows.empty() ? 0 : std::stoi(rows[0][0]);
}

} // namespace dm::learn