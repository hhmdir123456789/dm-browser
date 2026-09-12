#pragma once
#include <optional>
#include <utility>
#include "common/Error.h"

template <typename T>
class Result {
public:
    Result(T v) : value_(std::move(v)) {}
    Result(Error e) : error_(std::move(e)) {}

    bool ok() const { return !error_.has_value(); }
    const T& value() const { return *value_; }
    const Error& error() const { return *error_; }

    static Result ok(T v) { return Result(std::move(v)); }
    static Result fail(Error e) { return Result(std::move(e)); }

private:
    std::optional<T> value_;
    std::optional<Error> error_;
};
