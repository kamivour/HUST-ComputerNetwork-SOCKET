/**
 * @file Database.cpp
 * @brief Implementation of SQLite database operations
 */

#include "Database.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <functional>

Database& Database::getInstance() {
    static Database instance;
    return instance;
}

Database::Database() : db_(nullptr), initialized_(false) {}

Database::~Database() {
    close();
}

bool Database::initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return true;
    }

    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database] Error opening database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    if (!createTables()) {
        std::cerr << "[Database] Error creating tables" << std::endl;
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    initialized_ = true;
    std::cout << "[Database] Initialized successfully: " << dbPath << std::endl;
    return true;
}

void Database::close() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
        initialized_ = false;
        std::cout << "[Database] Connection closed" << std::endl;
    }
}

bool Database::createTables() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password TEXT NOT NULL,
            display_name TEXT,
            role INTEGER DEFAULT 0,
            is_banned INTEGER DEFAULT 0,
            is_muted INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );

        CREATE INDEX IF NOT EXISTS idx_username ON users(username);

        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sender TEXT NOT NULL,
            receiver TEXT,
            content TEXT NOT NULL,
            message_type TEXT NOT NULL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        );

        CREATE INDEX IF NOT EXISTS idx_messages_sender ON messages(sender);
        CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON messages(timestamp);
    )";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        std::cerr << "[Database] SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    // Add new columns if they don't exist (for migration from old schema)
    const char* migrations[] = {
        "ALTER TABLE users ADD COLUMN role INTEGER DEFAULT 0",
        "ALTER TABLE users ADD COLUMN is_banned INTEGER DEFAULT 0",
        "ALTER TABLE users ADD COLUMN is_muted INTEGER DEFAULT 0"
    };

    for (const char* migration : migrations) {
        sqlite3_exec(db_, migration, nullptr, nullptr, nullptr);
        // Ignore errors (column may already exist)
    }

    // Create first admin if no admin exists
    const char* checkAdmin = "SELECT COUNT(*) FROM users WHERE role = 1";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, checkAdmin, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 0) {
            // No admin exists, make first user admin if exists
            sqlite3_finalize(stmt);
            const char* makeAdmin = "UPDATE users SET role = 1 WHERE id = (SELECT MIN(id) FROM users)";
            sqlite3_exec(db_, makeAdmin, nullptr, nullptr, nullptr);
        } else {
            sqlite3_finalize(stmt);
        }
    }

    return true;
}

std::string Database::hashPassword(const std::string& password) {
    // Simple hash using std::hash (for demo purposes)
    // In production, use bcrypt, argon2, or similar
    std::hash<std::string> hasher;
    size_t hash = hasher(password + "chat_salt_2024");

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return oss.str();
}

bool Database::registerUser(const std::string& username, const std::string& password,
                            const std::string& displayName) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return false;
    }

    // Check if username already exists
    const char* checkSql = "SELECT COUNT(*) FROM users WHERE username = ?";
    sqlite3_stmt* checkStmt;

    if (sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(checkStmt, 1, username.c_str(), -1, SQLITE_STATIC);

    int count = 0;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        count = sqlite3_column_int(checkStmt, 0);
    }
    sqlite3_finalize(checkStmt);

    if (count > 0) {
        std::cout << "[Database] Username already exists: " << username << std::endl;
        return false;
    }

    // Insert new user
    const char* insertSql = "INSERT INTO users (username, password, display_name) VALUES (?, ?, ?)";
    sqlite3_stmt* insertStmt;

    if (sqlite3_prepare_v2(db_, insertSql, -1, &insertStmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string hashedPassword = hashPassword(password);
    std::string dispName = displayName.empty() ? username : displayName;

    sqlite3_bind_text(insertStmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insertStmt, 2, hashedPassword.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insertStmt, 3, dispName.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(insertStmt) == SQLITE_DONE);
    sqlite3_finalize(insertStmt);

    if (success) {
        std::cout << "[Database] User registered: " << username << std::endl;
    }

    return success;
}

bool Database::authenticateUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return false;
    }

    const char* sql = "SELECT password FROM users WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    bool authenticated = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* storedHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string hashedInput = hashPassword(password);
        authenticated = (hashedInput == storedHash);
    }

    sqlite3_finalize(stmt);
    return authenticated;
}

bool Database::changePassword(const std::string& username, const std::string& oldPassword,
                              const std::string& newPassword) {
    // First authenticate with old password
    if (!authenticateUser(username, oldPassword)) {
        std::cout << "[Database] Password change failed - wrong old password: " << username << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "UPDATE users SET password = ?, updated_at = CURRENT_TIMESTAMP WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string hashedPassword = hashPassword(newPassword);
    sqlite3_bind_text(stmt, 1, hashedPassword.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db_) > 0);
    sqlite3_finalize(stmt);

    if (success) {
        std::cout << "[Database] Password changed for: " << username << std::endl;
    }

    return success;
}

bool Database::userExists(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return false;
    }

    const char* sql = "SELECT COUNT(*) FROM users WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count > 0;
}

std::string Database::getDisplayName(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return "";
    }

    const char* sql = "SELECT display_name FROM users WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return "";
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    std::string displayName;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (name) {
            displayName = name;
        }
    }

    sqlite3_finalize(stmt);
    return displayName;
}

bool Database::updateDisplayName(const std::string& username, const std::string& displayName) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return false;
    }

    const char* sql = "UPDATE users SET display_name = ?, updated_at = CURRENT_TIMESTAMP WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, displayName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db_) > 0);
    sqlite3_finalize(stmt);

    if (success) {
        std::cout << "[Database] Display name updated for: " << username << std::endl;
    }

    return success;
}

// ========== Role Management ==========

int Database::getUserRole(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return -1;
    }

    const char* sql = "SELECT role FROM users WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    int role = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        role = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return role;
}

bool Database::isAdmin(const std::string& username) {
    return getUserRole(username) == 1;
}

bool Database::setUserRole(const std::string& username, int role) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return false;
    }

    const char* sql = "UPDATE users SET role = ?, updated_at = CURRENT_TIMESTAMP WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, role);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db_) > 0);
    sqlite3_finalize(stmt);

    if (success) {
        std::cout << "[Database] Role updated for " << username << " to " << role << std::endl;
    }

    return success;
}

// ========== Ban Management ==========

bool Database::banUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return false;
    }

    const char* sql = "UPDATE users SET is_banned = 1, updated_at = CURRENT_TIMESTAMP WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db_) > 0);
    sqlite3_finalize(stmt);

    if (success) {
        std::cout << "[Database] User banned: " << username << std::endl;
    }

    return success;
}

bool Database::unbanUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return false;
    }

    const char* sql = "UPDATE users SET is_banned = 0, updated_at = CURRENT_TIMESTAMP WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db_) > 0);
    sqlite3_finalize(stmt);

    if (success) {
        std::cout << "[Database] User unbanned: " << username << std::endl;
    }

    return success;
}

bool Database::isBanned(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return false;
    }

    const char* sql = "SELECT is_banned FROM users WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    bool banned = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        banned = sqlite3_column_int(stmt, 0) != 0;
    }

    sqlite3_finalize(stmt);
    return banned;
}

std::vector<std::string> Database::getBannedUsers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> users;

    if (!initialized_ || !db_) {
        return users;
    }

    const char* sql = "SELECT username FROM users WHERE is_banned = 1";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return users;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (username) {
            users.push_back(username);
        }
    }

    sqlite3_finalize(stmt);
    return users;
}

// ========== Mute Management ==========

bool Database::muteUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return false;
    }

    const char* sql = "UPDATE users SET is_muted = 1, updated_at = CURRENT_TIMESTAMP WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db_) > 0);
    sqlite3_finalize(stmt);

    if (success) {
        std::cout << "[Database] User muted: " << username << std::endl;
    }

    return success;
}

bool Database::unmuteUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return false;
    }

    const char* sql = "UPDATE users SET is_muted = 0, updated_at = CURRENT_TIMESTAMP WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(db_) > 0);
    sqlite3_finalize(stmt);

    if (success) {
        std::cout << "[Database] User unmuted: " << username << std::endl;
    }

    return success;
}

bool Database::isMuted(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return false;
    }

    const char* sql = "SELECT is_muted FROM users WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    bool muted = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        muted = sqlite3_column_int(stmt, 0) != 0;
    }

    sqlite3_finalize(stmt);
    return muted;
}

std::vector<std::string> Database::getMutedUsers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> users;

    if (!initialized_ || !db_) {
        return users;
    }

    const char* sql = "SELECT username FROM users WHERE is_muted = 1";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return users;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (username) {
            users.push_back(username);
        }
    }

    sqlite3_finalize(stmt);
    return users;
}

// ========== User Info ==========

UserInfo Database::getUserInfo(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    UserInfo info;
    info.username = username;

    if (!initialized_ || !db_) {
        return info;
    }

    const char* sql = "SELECT display_name, role, is_banned, is_muted, created_at FROM users WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return info;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* displayName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        info.displayName = displayName ? displayName : username;
        info.role = sqlite3_column_int(stmt, 1);
        info.isBanned = sqlite3_column_int(stmt, 2) != 0;
        info.isMuted = sqlite3_column_int(stmt, 3) != 0;
        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        info.createdAt = createdAt ? createdAt : "";
    }

    sqlite3_finalize(stmt);
    return info;
}

std::vector<UserInfo> Database::getAllUsers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UserInfo> users;

    if (!initialized_ || !db_) {
        return users;
    }

    const char* sql = "SELECT username, display_name, role, is_banned, is_muted, created_at FROM users ORDER BY username";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return users;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        UserInfo info;
        const char* username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* displayName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        info.username = username ? username : "";
        info.displayName = displayName ? displayName : info.username;
        info.role = sqlite3_column_int(stmt, 2);
        info.isBanned = sqlite3_column_int(stmt, 3) != 0;
        info.isMuted = sqlite3_column_int(stmt, 4) != 0;

        const char* createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        info.createdAt = createdAt ? createdAt : "";

        users.push_back(info);
    }

    sqlite3_finalize(stmt);
    return users;
}

// ========== Message Logging ==========

bool Database::logMessage(const std::string& sender, const std::string& receiver,
                          const std::string& content, const std::string& messageType) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return false;
    }

    const char* sql = "INSERT INTO messages (sender, receiver, content, message_type) VALUES (?, ?, ?, ?)";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, sender.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, receiver.empty() ? nullptr : receiver.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, messageType.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

std::vector<std::string> Database::getRecentMessages(int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> messages;

    if (!initialized_ || !db_) {
        return messages;
    }

    const char* sql = "SELECT id, sender, receiver, content, message_type, timestamp "
                      "FROM messages ORDER BY timestamp DESC LIMIT ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return messages;
    }

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::ostringstream oss;
        oss << "{"
            << "\"id\":" << sqlite3_column_int(stmt, 0) << ","
            << "\"sender\":\"" << (sqlite3_column_text(stmt, 1) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)) : "") << "\","
            << "\"receiver\":\"" << (sqlite3_column_text(stmt, 2) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)) : "") << "\","
            << "\"content\":\"" << (sqlite3_column_text(stmt, 3) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)) : "") << "\","
            << "\"messageType\":\"" << (sqlite3_column_text(stmt, 4) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)) : "") << "\","
            << "\"timestamp\":\"" << (sqlite3_column_text(stmt, 5) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)) : "") << "\""
            << "}";
        messages.push_back(oss.str());
    }

    sqlite3_finalize(stmt);
    return messages;
}

int Database::getMessageCount() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !db_) {
        return 0;
    }

    const char* sql = "SELECT COUNT(*) FROM messages";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}
