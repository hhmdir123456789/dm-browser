#pragma once
#include <mutex>
#include <string>
#include <vector>
#include "common/Result.h"

// SQLite 封装：单连接，互斥保护
class Database {
public:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // 打开/关闭
    Result<bool> open(const std::string& path);
    Result<bool> close();

    // 执行写操作
    Result<bool> exec(const std::string& sql,
                      const std::vector<std::string>& params = {});

    // 查询：返回行，每行为列字符串
    std::vector<std::vector<std::string>> query(
        const std::string& sql,
        const std::vector<std::string>& params = {});

    // 最后插入 rowid
    int64_t lastInsertId();

    // 事务
    Result<bool> begin();
    Result<bool> commit();
    Result<bool> rollback();

    bool isOpen() const { return db_ != nullptr; }
    std::string lastError() const { return lastError_; }

private:
    void* db_{nullptr};   // sqlite3*
    mutable std::mutex mu_;
    std::string lastError_;
};