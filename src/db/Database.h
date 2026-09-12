#pragma once
#include <mutex>
#include <string>
#include <vector>
#include "common/Result.h"

class Database {
public:
    Database();
    ~Database();

    Result<bool> open(const std::string& path);
    Result<bool> close();

    Result<bool> exec(const std::string& sql,
                      const std::vector<std::string>& params = {});

    std::vector<std::vector<std::string>> query(
        const std::string& sql,
        const std::vector<std::string>& params = {});

    int64_t lastInsertId();

    Result<bool> begin();
    Result<bool> commit();
    Result<bool> rollback();

    bool isOpen() const { return db_ != nullptr; }
    std::string lastError() const { return lastError_; }

private:
    void* db_{nullptr};
    mutable std::mutex mu_;
    std::string lastError_;
};
