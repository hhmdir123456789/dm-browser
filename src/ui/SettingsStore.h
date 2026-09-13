#pragma once
#include <string>
#include <mutex>
#include <vector>
#include <windows.h>
#include "db/Database.h"

namespace dm::ui {

// 简单 key-value 存储，落盘到 dm_ui.db 的 settings 表
class SettingsStore {
public:
    explicit SettingsStore(Database& db) : db_(db) {}

    bool init() {
        auto r = db_.exec(
            "CREATE TABLE IF NOT EXISTS settings ("
            "key TEXT PRIMARY KEY, value TEXT)");
        if (!r.isOk()) return false;
        r = db_.exec(
            "CREATE TABLE IF NOT EXISTS history ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "url TEXT, title TEXT, visited_at INTEGER)");
        return r.isOk();
    }

    std::string get(const std::string& key, const std::string& def = "") {
        std::lock_guard lock(mu_);
        auto rows = db_.query("SELECT value FROM settings WHERE key = ?", {key});
        if (rows.empty()) return def;
        return rows[0][0];
    }

    void set(const std::string& key, const std::string& value) {
        std::lock_guard lock(mu_);
        db_.exec("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)",
                 {key, value});
    }

    // 历史记录
    void addHistory(const std::string& url, const std::string& title) {
        if (url.empty() || url == "about:blank") return;
        std::lock_guard lock(mu_);
        db_.exec("INSERT INTO history (url, title, visited_at) VALUES (?, ?, ?)",
                 {url, title, std::to_string((long long)GetTickCount64())});
        // 只保留最近 1000 条
        db_.exec("DELETE FROM history WHERE id NOT IN "
                 "(SELECT id FROM history ORDER BY id DESC LIMIT 1000)");
    }

    struct HistoryItem {
        std::string url;
        std::string title;
        long long visitedAt;
    };

    std::vector<HistoryItem> recentHistory(int limit = 50) {
        std::lock_guard lock(mu_);
        auto rows = db_.query(
            "SELECT url, title, visited_at FROM history "
            "ORDER BY id DESC LIMIT ?",
            {std::to_string(limit)});
        std::vector<HistoryItem> out;
        for (auto& r : rows) {
            long long t = 0;
            try { t = std::stoll(r[2]); } catch (...) {}
            out.push_back({r[0], r[1], t});
        }
        return out;
    }

private:
    Database& db_;
    std::mutex mu_;
};

} // namespace dm::ui