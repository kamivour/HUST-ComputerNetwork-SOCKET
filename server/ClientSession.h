/**
 * @file ClientSession.h
 * @brief Represents a connected client session
 */

#ifndef CLIENT_SESSION_H
#define CLIENT_SESSION_H

#include <string>
#include <atomic>
#include <mutex>
#include <chrono>
#include "../common/Protocol.h"

class Server;  // Forward declaration

class ClientSession {
public:
    /**
     * @brief Construct a new Client Session
     * @param socketFd Socket file descriptor
     * @param server Reference to parent server
     */
    ClientSession(int socketFd, Server* server);

    /**
     * @brief Destroy the Client Session
     */
    ~ClientSession();

    // Disable copy
    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;

    /**
     * @brief Get socket file descriptor
     * @return Socket fd
     */
    int getSocketFd() const { return socketFd_; }

    /**
     * @brief Get username (empty if not logged in)
     * @return Username
     */
    std::string getUsername() const;

    /**
     * @brief Get display name
     * @return Display name
     */
    std::string getDisplayName() const;

    /**
     * @brief Check if client is authenticated
     * @return true if logged in
     */
    bool isAuthenticated() const { return authenticated_; }

    /**
     * @brief Set authenticated status with username
     * @param username Username
     * @param displayName Display name
     */
    void setAuthenticated(const std::string& username, const std::string& displayName);

    /**
     * @brief Clear authentication
     */
    void clearAuthentication();

    /**
     * @brief Check if session is active
     * @return true if active
     */
    bool isActive() const { return active_; }

    /**
     * @brief Mark session as inactive
     */
    void setInactive() { active_ = false; }

    /**
     * @brief Send a message to this client
     * @param msg Message to send
     * @return true if send successful
     */
    bool sendMessage(const Protocol::Message& msg);

    /**
     * @brief Process received data
     * @param data Pointer to received data
     * @param length Length of data
     */
    void processData(const uint8_t* data, size_t length);

    /**
     * @brief Get client address string
     * @return Client address
     */
    std::string getAddress() const { return address_; }

    /**
     * @brief Set client address
     * @param addr Address string
     */
    void setAddress(const std::string& addr) { address_ = addr; }

private:
    /**
     * @brief Handle a complete message
     * @param msg Received message
     */
    void handleMessage(const Protocol::Message& msg);

    /**
     * @brief Handle REGISTER request
     * @param msg Message
     */
    void handleRegister(const Protocol::Message& msg);

    /**
     * @brief Handle LOGIN request
     * @param msg Message
     */
    void handleLogin(const Protocol::Message& msg);

    /**
     * @brief Handle LOGOUT request
     * @param msg Message
     */
    void handleLogout(const Protocol::Message& msg);

    /**
     * @brief Handle CHANGE_PASSWORD request
     * @param msg Message
     */
    void handleChangePassword(const Protocol::Message& msg);

    /**
     * @brief Handle MSG_GLOBAL request
     * @param msg Message
     */
    void handleGlobalMessage(const Protocol::Message& msg);

    /**
     * @brief Handle MSG_PRIVATE request
     * @param msg Message
     */
    void handlePrivateMessage(const Protocol::Message& msg);

    // ========== Admin Commands ==========

    /**
     * @brief Handle KICK_USER request (admin only)
     */
    void handleKickUser(const Protocol::Message& msg);

    /**
     * @brief Handle BAN_USER request (admin only)
     */
    void handleBanUser(const Protocol::Message& msg);

    /**
     * @brief Handle UNBAN_USER request (admin only)
     */
    void handleUnbanUser(const Protocol::Message& msg);

    /**
     * @brief Handle MUTE_USER request (admin only)
     */
    void handleMuteUser(const Protocol::Message& msg);

    /**
     * @brief Handle UNMUTE_USER request (admin only)
     */
    void handleUnmuteUser(const Protocol::Message& msg);

    /**
     * @brief Handle PROMOTE_USER request (admin only)
     */
    void handlePromoteUser(const Protocol::Message& msg);

    /**
     * @brief Handle DEMOTE_USER request (admin only)
     */
    void handleDemoteUser(const Protocol::Message& msg);

    /**
     * @brief Handle GET_ALL_USERS request (admin only)
     */
    void handleGetAllUsers(const Protocol::Message& msg);

    /**
     * @brief Handle GET_BANNED_LIST request (admin only)
     */
    void handleGetBannedList(const Protocol::Message& msg);

    /**
     * @brief Handle GET_MUTED_LIST request (admin only)
     */
    void handleGetMutedList(const Protocol::Message& msg);

    /**
     * @brief Handle USER_INFO request
     */
    void handleUserInfo(const Protocol::Message& msg);

    /**
     * @brief Check if current user is admin
     */
    bool isAdmin() const;

    /**
     * @brief Check rate limit for sending messages
     * @return true if within rate limit, false if rate limit exceeded
     */
    bool checkRateLimit();

    int socketFd_;
    Server* server_;
    std::string username_;
    std::string displayName_;
    std::string address_;
    std::atomic<bool> authenticated_;
    std::atomic<bool> active_;
    Protocol::MessageBuffer buffer_;
    mutable std::mutex mutex_;
    std::mutex sendMutex_;

    // Rate limiting
    static constexpr int RATE_LIMIT_MAX_MESSAGES = 10;  // Max messages per window
    static constexpr int RATE_LIMIT_WINDOW_SECONDS = 1; // Time window in seconds
    std::chrono::steady_clock::time_point rateLimitWindowStart_;
    int rateLimitMessageCount_;
    std::mutex rateLimitMutex_;
};

#endif // CLIENT_SESSION_H
