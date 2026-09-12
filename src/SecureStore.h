#pragma once
#include <string>
#include <vector>
#include "common/Types.h"
#include "db/Database.h"

struct Credential {
    std::string origin;
    std::string username;
    std::string encryptedPassword;
    int64_t createdAt{0};
};

class SecureStore {
public:
    explicit SecureStore(Database& db) : db_(db) {}

    bool store(const std::string& origin, const std::string& user,
               const std::string& plain) {
        db_.exec(
            "INSERT OR REPLACE INTO credentials (origin, username, encrypted_password, created_at) "
            "VALUES (?,?,?,?)",
            {origin, user, "enc:" + plain, std::to_string(nowMs())});
        return true;
    }

    std::vector<Credential> lookup(const std::string& origin) const {
        auto rows = db_.query(
            "SELECT origin, username, encrypted_password, created_at "
            "FROM credentials WHERE origin = ?", {origin});
        std::vector<Credential> out;
        for (auto& r : rows)
            out.push_back({r[0], r[1], r[2], std::stoll(r[3])});
        return out;
    }

    bool remove(const std::string& origin, const std::string& user) {
        db_.exec("DELETE FROM credentials WHERE origin = ? AND username = ?",
                 {origin, user});
        return true;
    }

    size_t size() const {
        auto rows = db_.query("SELECT COUNT(*) FROM credentials");
        return rows.empty() ? 0 : std::stoul(rows[0][0]);
    }

private:
    Database& db_;
};
