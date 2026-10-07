#pragma once

#include <QString>
#include <utility>

// A value, or the reason there isn't one.
template <typename T>
struct Result {
    T value{};
    QString error;

    bool ok() const { return error.isEmpty(); }

    static Result success(T v) { return {std::move(v), {}}; }
    static Result failure(QString e) { return {T{}, std::move(e)}; }
};
