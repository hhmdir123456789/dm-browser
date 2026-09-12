#pragma once
#include <optional>
#include <string>
#include <vector>
#include "common/Types.h"
#include "common/Error.h"
#include "common/Result.h"
#include "db/Database.h"

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
        auto rows = db_.query("SELECT COUNT(*) FROM bookmarks");
        size_t n = rows.empty() ? 0 : std::stoul(rows[0][0]);
        if (n >= quota_)
            return Result<BookmarkId>::fail(
                Error::scope(ErrCode::QuotaExceeded, "bookmark quota exceeded"));
        db_.exec("INSERT INTO bookmarks (title, url, created_at) VALUES (?,?,?)",
                 {title, url, std::to_string(nowMs())});
        return Result<BookmarkId>::ok(static_cast<BookmarkId>(db_.lastInsertId()));
    }

    bool remove(BookmarkId id) {
        db_.exec("DELETE FROM bookmarks WHERE id = ?", {std::to_string(id)});
        return true;
    }

    std::vector<Bookmark> list() const {
        auto rows = db_.query("SELECT id, title, url, created_at FROM bookmarks ORDER BY id");
        std::vector<Bookmark> out;
        for (auto& r : rows)
            out.push_back({std::stoi(r[0]), r[1], r[2], std::stoll(r[3])});
        return out;
    }

    size_t size() const {
        auto rows = db_.query("SELECT COUNT(*) FROM bookmarks");
        return rows.empty() ? 0 : std::stoul(rows[0][0]);
    }

private:
    Database& db_;
    size_t quota_{1000};
};

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
        if (n >= quota_)
            return Result<DownloadId>::fail(
                Error::scope(ErrCode::QuotaExceeded, "download quota exceeded"));
        db_.exec("INSERT INTO downloads (url, filename, state, created_at) VALUES (?,?,?,?)",
                 {url, fn, "pending", std::to_string(nowMs())});
        return Result<DownloadId>::ok(static_cast<DownloadId>(db_.lastInsertId()));
    }

    bool update(DownloadId id, const std::string& state, int64_t rec, int64_t total) {
        db_.exec("UPDATE downloads SET state = ?, bytes_received = ?, total_bytes = ? WHERE id = ?",
                 {state, std::to_string(rec), std::to_string(total), std::to_string(id)});
        return true;
    }

    std::vector<DownloadItem> list() const {
        auto rows = db_.query(
            "SELECT id, url, filename, state, bytes_received, total_bytes, created_at "
            "FROM downloads ORDER BY id");
        std::vector<DownloadItem> out;
        for (auto& r : rows)
            out.push_back({std::stoi(r[0]), r[1], r[2], r[3],
                           std::stoll(r[4]), std::stoll(r[5]), std::stoll(r[6])});
        return out;
    }

    size_t size() const {
        auto rows = db_.query("SELECT COUNT(*) FROM downloads");
        return rows.empty() ? 0 : std::stoul(rows[0][0]);
    }

private:
    Database& db_;
    size_t quota_{1000};
};
