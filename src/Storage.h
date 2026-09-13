#pragma once
#include <optional>
#include <string>
#include <vector>
#include "common/Types.h"
#include "common/Error.h"
#include "common/Result.h"
#include "db/Database.h"

// ============================================================
// 书签
// ============================================================
struct Bookmark {
    BookmarkId id{0};
    std::string title;
    std::string url;
    int64_t createdAt{0};
};

class BookmarkStore {
public:
    explicit BookmarkStore(Database& db) : db_(db) {}

    Result<BookmarkId> add(const std::string& title, const std::string& url) {
        // 去重：URL 已存在则返回已有 ID
        auto existing = db_.query(
            "SELECT id FROM bookmarks WHERE url = ?", {url});
        if (!existing.empty()) {
            return Result<BookmarkId>::ok(
                static_cast<BookmarkId>(std::stoi(existing[0][0])));
        }

        // 配额检查
        auto rows = db_.query("SELECT COUNT(*) FROM bookmarks");
        size_t n = rows.empty() ? 0 : std::stoul(rows[0][0]);
        if (n >= quota_) {
            return Result<BookmarkId>::fail(
                Error::scope(ErrCode::QuotaExceeded, "bookmark quota exceeded"));
        }

        db_.exec("INSERT INTO bookmarks (title, url, created_at) VALUES (?,?,?)",
                 {title, url, std::to_string(nowMs())});
        return Result<BookmarkId>::ok(
            static_cast<BookmarkId>(db_.lastInsertId()));
    }

    bool remove(BookmarkId id) {
        db_.exec("DELETE FROM bookmarks WHERE id = ?",
                 {std::to_string(id)});
        return true;
    }

    std::optional<Bookmark> get(BookmarkId id) const {
        auto rows = db_.query(
            "SELECT id, title, url, created_at FROM bookmarks WHERE id = ?",
            {std::to_string(id)});
        if (rows.empty()) return std::nullopt;
        return rowToBookmark(rows[0]);
    }

    std::vector<Bookmark> list() const {
        auto rows = db_.query(
            "SELECT id, title, url, created_at FROM bookmarks "
            "ORDER BY created_at DESC");
        std::vector<Bookmark> out;
        for (auto& r : rows) out.push_back(rowToBookmark(r));
        return out;
    }

    size_t size() const {
        auto rows = db_.query("SELECT COUNT(*) FROM bookmarks");
        return rows.empty() ? 0 : std::stoul(rows[0][0]);
    }

private:
    static Bookmark rowToBookmark(const std::vector<std::string>& r) {
        return Bookmark{std::stoi(r[0]), r[1], r[2], std::stoll(r[3])};
    }

    Database& db_;
    size_t quota_{1000};
};

// ============================================================
// 下载
// ============================================================
struct DownloadItem {
    DownloadId id{0};
    std::string url;
    std::string filename;
    std::string state;
    int64_t bytesReceived{0};
    int64_t totalBytes{0};
    int64_t createdAt{0};
};

class DownloadStore {
public:
    explicit DownloadStore(Database& db) : db_(db) {}

    Result<DownloadId> create(const std::string& url, const std::string& fn) {
        auto rows = db_.query("SELECT COUNT(*) FROM downloads");
        size_t n = rows.empty() ? 0 : std::stoul(rows[0][0]);
        if (n >= quota_) {
            return Result<DownloadId>::fail(
                Error::scope(ErrCode::QuotaExceeded, "download quota exceeded"));
        }
        db_.exec(
            "INSERT INTO downloads (url, filename, state, created_at) "
            "VALUES (?,?,?,?)",
            {url, fn, "pending", std::to_string(nowMs())});
        return Result<DownloadId>::ok(
            static_cast<DownloadId>(db_.lastInsertId()));
    }

    bool update(DownloadId id, const std::string& state,
                int64_t rec, int64_t total) {
        db_.exec(
            "UPDATE downloads SET state = ?, bytes_received = ?, "
            "total_bytes = ? WHERE id = ?",
            {state, std::to_string(rec), std::to_string(total),
             std::to_string(id)});
        return true;
    }

    std::optional<DownloadItem> get(DownloadId id) const {
        auto rows = db_.query(
            "SELECT id, url, filename, state, bytes_received, "
            "total_bytes, created_at FROM downloads WHERE id = ?",
            {std::to_string(id)});
        if (rows.empty()) return std::nullopt;
        return rowToItem(rows[0]);
    }

    std::vector<DownloadItem> list() const {
        auto rows = db_.query(
            "SELECT id, url, filename, state, bytes_received, "
            "total_bytes, created_at FROM downloads ORDER BY id");
        std::vector<DownloadItem> out;
        for (auto& r : rows) out.push_back(rowToItem(r));
        return out;
    }

    size_t size() const {
        auto rows = db_.query("SELECT COUNT(*) FROM downloads");
        return rows.empty() ? 0 : std::stoul(rows[0][0]);
    }

private:
    static DownloadItem rowToItem(const std::vector<std::string>& r) {
        return DownloadItem{std::stoi(r[0]), r[1], r[2], r[3],
                            std::stoll(r[4]), std::stoll(r[5]), std::stoll(r[6])};
    }

    Database& db_;
    size_t quota_{1000};
};