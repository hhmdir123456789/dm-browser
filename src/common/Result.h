#pragma once
#include <optional>
#include <utility>
#include "common/Error.h"

// ============================================================
// Result<T>：带值的成功/失败结果
// ============================================================
template <typename T>
class Result {
public:
    Result(T v) : value_(std::move(v)) {}
    Result(Error e) : error_(std::move(e)) {}

    // 实例判断方法改名为 isOk()，避免与静态工厂 ok() 同名冲突
    bool isOk() const { return !error_.has_value(); }
    const T& value() const { return *value_; }
    T& value() { return *value_; }
    const Error& error() const { return *error_; }

    // 静态工厂：保留旧名字，所有调用处一行都不用改
    static Result ok(T v)       { return Result(std::move(v)); }
    static Result fail(Error e) { return Result(std::move(e)); }

    // 新名字也保留，方便以后慢慢迁移
    static Result Ok(T v)       { return Result(std::move(v)); }
    static Result Fail(Error e) { return Result(std::move(e)); }

private:
    std::optional<T> value_;
    std::optional<Error> error_;
};

// ============================================================
// Status：不带值的成功/失败结果
// ============================================================
class Status {
public:
    Status() = default;
    Status(Error e) : error_(std::move(e)) {}

    bool isOk() const { return !error_.has_value(); }
    const Error& error() const { return *error_; }

    static Status ok()          { return {}; }
    static Status fail(Error e) { return Status(std::move(e)); }

    static Status Ok()          { return {}; }
    static Status Fail(Error e) { return Status(std::move(e)); }

private:
    std::optional<Error> error_;
};