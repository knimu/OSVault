#pragma once
#include <string>
#include <utility>

namespace osvault {

template <typename T>
class Result {
public:
    static Result<T> ok(T value) {
        return Result<T>(true, std::move(value), "");
    }

    static Result<T> failure(std::string error_message, int code = 400) {
        return Result<T>(false, T{}, std::move(error_message), code);
    }

    bool is_ok() const noexcept { return ok_; }
    bool is_error() const noexcept { return !ok_; }

    const T& value() const { return value_; }
    T& value() { return value_; }

    const std::string& error() const noexcept { return error_; }
    int code() const noexcept { return code_; }

private:
    Result(bool ok, T val, std::string err, int code = 200)
        : ok_(ok), value_(std::move(val)), error_(std::move(err)), code_(code) {}

    bool ok_;
    T value_;
    std::string error_;
    int code_{200};
};

template <>
class Result<void> {
public:
    static Result<void> ok() {
        return Result<void>(true, "", 200);
    }

    static Result<void> failure(std::string error_message, int code = 400) {
        return Result<void>(false, std::move(error_message), code);
    }

    bool is_ok() const noexcept { return ok_; }
    bool is_error() const noexcept { return !ok_; }

    const std::string& error() const noexcept { return error_; }
    int code() const noexcept { return code_; }

private:
    Result(bool ok, std::string err, int code)
        : ok_(ok), error_(std::move(err)), code_(code) {}

    bool ok_;
    std::string error_;
    int code_{200};
};

} // namespace osvault
