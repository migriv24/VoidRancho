/* sql.hpp — the prepared statement every Rancho module uses (internal). */
#pragma once

#include "sqlite3.h"

#include <string>
#include <string_view>

namespace rancho {

/* A prepared statement, reused: bind, step, reset. */
class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) : db_(db) { sqlite3_prepare_v2(db, sql, -1, &st_, nullptr); }
    Stmt(sqlite3* db, const std::string& sql) : Stmt(db, sql.c_str()) {}
    ~Stmt() { sqlite3_finalize(st_); }
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;
    bool ok() const { return st_ != nullptr; }
    Stmt& text(int i, std::string_view v) {
        sqlite3_bind_text(st_, i, v.data() ? v.data() : "", (int)v.size(), SQLITE_TRANSIENT);
        return *this;
    }
    Stmt& blob(int i, std::string_view v) {
        sqlite3_bind_blob(st_, i, v.data() ? v.data() : "", (int)v.size(), SQLITE_TRANSIENT);
        return *this;
    }
    Stmt& null(int i) {
        sqlite3_bind_null(st_, i);
        return *this;
    }
    Stmt& integer(int i, long long v) {
        sqlite3_bind_int64(st_, i, v);
        return *this;
    }
    Stmt& real(int i, double v) {
        sqlite3_bind_double(st_, i, v);
        return *this;
    }
    // one statement that returns no rows; resets for the next use
    bool run() {
        const int rc = sqlite3_step(st_);
        sqlite3_reset(st_);
        sqlite3_clear_bindings(st_);
        return rc == SQLITE_DONE || rc == SQLITE_ROW;
    }
    bool row() { return sqlite3_step(st_) == SQLITE_ROW; }
    void reset() {
        sqlite3_reset(st_);
        sqlite3_clear_bindings(st_);
    }
    std::string_view col(int i) {
        const auto* p = (const char*)sqlite3_column_text(st_, i);
        return p ? std::string_view(p, (std::size_t)sqlite3_column_bytes(st_, i)) : std::string_view();
    }
    std::string col_blob(int i) {
        const auto* p = (const char*)sqlite3_column_blob(st_, i);
        return p ? std::string(p, (std::size_t)sqlite3_column_bytes(st_, i)) : std::string();
    }
    bool is_null(int i) { return sqlite3_column_type(st_, i) == SQLITE_NULL; }
    long long col_int(int i) { return sqlite3_column_int64(st_, i); }
    double col_real(int i) { return sqlite3_column_double(st_, i); }

private:
    sqlite3* db_;
    sqlite3_stmt* st_ = nullptr;
};

} // namespace rancho
