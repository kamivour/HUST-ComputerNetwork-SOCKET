/**
 * @file Database.h
 * @brief SQLite database interface for user management
 */

#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <mutex>
#include <sqlite3.h>

// User info structure
struct UserInfo {
    std::string username;
    std::string displayName;
    int role;           // 0 = member, 1 = admin
    bool isBanned;
    bool isMuted;
    std::string createdAt;
};

class Database {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to Database instance
     */
    static Database& getInstance();

    // Delete copy constructor and assignment operator
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /**
     * @brief Initialize database connection and create tables
     * @param dbPath Path to SQLite database file
     * @return true if successful
     */
    bool initialize(const std::string& dbPath = "chat_server.db");

    /**
     * @brief Close database connection
     */
    void close();

    /**
     * @brief Register a new user
     * @param username Username (unique)
     * @param password Password (will be hashed)
     * @param displayName Display name (optional, defaults to username)
     * @return true if registration successful, false if username exists
     */
    bool registerUser(const std::string& username, const std::string& password,
                      const std::string& displayName = "");

    /**
     * @brief Authenticate user login
     * @param username Username
     * @param password Password
     * @return true if credentials are valid
     */
    bool authenticateUser(const std::string& username, const std::string& password);

    /**
     * @brief Change user password
     * @param username Username
     * @param oldPassword Current password (for verification)
     * @param newPassword New password
     * @return true if password changed successfully
     */
    bool changePassword(const std::string& username, const std::string& oldPassword,
                        const std::string& newPassword);

    /**
     * @brief Check if username exists
     * @param username Username to check
     * @return true if username exists
     */
    bool userExists(const std::string& username);

    /**
     * @brief Get user's display name
     * @param username Username
     * @return Display name or empty string if not found
     */
    std::string getDisplayName(const std::string& username);

    /**
     * @brief Update user's display name
     * @param username Username
     * @param displayName New display name
     * @return true if updated successfully
     */
    bool updateDisplayName(const std::string& username, const std::string& displayName);

    // ========== Role Management ==========

    /**
     * @brief Get user's role
     * @param username Username
     * @return 0 = member, 1 = admin, -1 = not found
     */
    int getUserRole(const std::string& username);

    /**
     * @brief Check if user is admin
     * @param username Username
     * @return true if admin
     */
    bool isAdmin(const std::string& username);

    /**
     * @brief Set user's role
     * @param username Username
     * @param role 0 = member, 1 = admin
     * @return true if successful
     */
    bool setUserRole(const std::string& username, int role);

    // ========== Ban Management ==========

    /**
     * @brief Ban a user
     * @param username Username to ban
     * @return true if successful
     */
    bool banUser(const std::string& username);

    /**
     * @brief Unban a user
     * @param username Username to unban
     * @return true if successful
     */
    bool unbanUser(const std::string& username);

    /**
     * @brief Check if user is banned
     * @param username Username
     * @return true if banned
     */
    bool isBanned(const std::string& username);

    /**
     * @brief Get list of banned users
     * @return Vector of banned usernames
     */
    std::vector<std::string> getBannedUsers();

    // ========== Mute Management ==========

    /**
     * @brief Mute a user
     * @param username Username to mute
     * @return true if successful
     */
    bool muteUser(const std::string& username);

    /**
     * @brief Unmute a user
     * @param username Username to unmute
     * @return true if successful
     */
    bool unmuteUser(const std::string& username);

    /**
     * @brief Check if user is muted
     * @param username Username
     * @return true if muted
     */
    bool isMuted(const std::string& username);

    /**
     * @brief Get list of muted users
     * @return Vector of muted usernames
     */
    std::vector<std::string> getMutedUsers();

    // ========== User Info ==========

    /**
     * @brief Get user information
     * @param username Username
     * @return UserInfo struct
     */
    UserInfo getUserInfo(const std::string& username);

    /**
     * @brief Get all registered users
     * @return Vector of UserInfo
     */
    std::vector<UserInfo> getAllUsers();

    // ========== Message Logging ==========

    /**
     * @brief Log a chat message to the database
     * @param sender Sender username
     * @param receiver Receiver username (empty for global messages)
     * @param content Message content
     * @param messageType "global" or "private"
     * @return true if successful
     */
    bool logMessage(const std::string& sender, const std::string& receiver,
                    const std::string& content, const std::string& messageType);

    /**
     * @brief Get recent messages from history
     * @param limit Maximum number of messages to retrieve
     * @return Vector of message records as JSON strings
     */
    std::vector<std::string> getRecentMessages(int limit = 50);

    /**
     * @brief Get message count for statistics
     * @return Total number of messages
     */
    int getMessageCount();

private:
    Database();
    ~Database();

    /**
     * @brief Create tables if not exist
     * @return true if successful
     */
    bool createTables();

    /**
     * @brief Simple hash function for password
     * @param password Plain text password
     * @return Hashed password
     */
    std::string hashPassword(const std::string& password);

    sqlite3* db_;
    std::mutex mutex_;
    bool initialized_;
};

#endif // DATABASE_H
