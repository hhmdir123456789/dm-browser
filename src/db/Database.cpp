#include "db/Database.h"
#include <sqlite3.h>

Database::Database() = default;

Database::~Database() {
    close();
}

Result<bool> Database::open(const std::string& path) {
    std::lock_guard lock(mu_);
    if (db_) return Result<bool>::Fail(Error::db("already open"));
    sqlite3* handle = nullptr;
    int rc = sqlite3_open(path.c_str(), &handle);
    if (rc != SQLITE_OK) {
        lastError_ = handle ? sqlite3_errmsg(handle) : "open failed";
        if (handle) sqlite3_close(handle);
        return Result<bool>::Fail(Error::db(lastError_));
    }
    db_ = handle;
    sqlite3_exec(handle, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    sqlite3_exec(handle, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
    return Result<bool>::Ok(true);
}

Result<bool> Database::close() {
    std::lock_guard lock(mu_);
    if (db_) {
        sqlite3_close(static_cast<sqlite3*>(db_));
        db_ = nullptr;
    }
    return Result<bool>::Ok(true);
}

Result<bool> Database::exec(const std::string& sql,
                            const std::vector<std::string>& params) {
    std::lock_guard lock(mu_);
    if (!db_) return Result<bool>::Fail(Error::db("not open"));

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql.c_str(),
                                 -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>::Fail(Error::db(sqlite3_errmsg(static_cast<sqlite3*>(db_))));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1),
                          params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        return Result<bool>::Fail(Error::db(sqlite3_errmsg(static_cast<sqlite3*>(db_))));
    }
    return Result<bool>::Ok(true);
}

std::vector<std::vector<std::string>> Database::query(
    const std::string& sql, const std::vector<std::string>& params) {
    std::lock_guard lock(mu_);
    std::vector<std::vector<std::string>> out;
    if (!db_) return out;

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql.c_str(),
                                 -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return out;

    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1),
                          params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::vector<std::string> row;
        int cols = sqlite3_column_count(stmt);
        for (int i = 0; i < cols; ++i) {
            const unsigned char* txt = sqlite3_column_text(stmt, i);
            row.push_back(txt ? reinterpret_cast<const char*>(txt) : "");
        }
        out.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    return out;
}

int64_t Database::lastInsertId() {
    std::lock_guard lock(mu_);
    if (!db_) return 0;
    return sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
}

Result<bool> Database::begin()    { return exec("BEGIN"); }
Result<bool> Database::commit()   { return exec("COMMIT"); }
Result<bool> Database::rollback() { return exec("ROLLBACK"); }