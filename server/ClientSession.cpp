/**
 * @file ClientSession.cpp
 * @brief Implementation of ClientSession
 */

#include "ClientSession.h"
#include "Server.h"
#include "Database.h"
#include "../thirdparty/json.hpp"
#include <iostream>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <unistd.h>
#endif

using json = nlohmann::json;

ClientSession::ClientSession(int socketFd, Server* server)
    : socketFd_(socketFd)
    , server_(server)
    , authenticated_(false)
    , active_(true)
    , rateLimitWindowStart_(std::chrono::steady_clock::now())
    , rateLimitMessageCount_(0) {
}

ClientSession::~ClientSession() {
    if (socketFd_ >= 0) {
#ifdef _WIN32
        closesocket(socketFd_);
#else
        close(socketFd_);
#endif
    }
}

std::string ClientSession::getUsername() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return username_;
}

std::string ClientSession::getDisplayName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return displayName_;
}

void ClientSession::setAuthenticated(const std::string& username, const std::string& displayName) {
    std::lock_guard<std::mutex> lock(mutex_);
    username_ = username;
    displayName_ = displayName;
    authenticated_ = true;
}

void ClientSession::clearAuthentication() {
    std::lock_guard<std::mutex> lock(mutex_);
    username_.clear();
    displayName_.clear();
    authenticated_ = false;
}

bool ClientSession::sendMessage(const Protocol::Message& msg) {
    std::lock_guard<std::mutex> lock(sendMutex_);

    if (!active_ || socketFd_ < 0) {
        return false;
    }

    std::vector<uint8_t> data = Protocol::serialize(msg);

    size_t totalSent = 0;
    while (totalSent < data.size()) {
        ssize_t sent = send(socketFd_, reinterpret_cast<const char*>(data.data() + totalSent),
                           data.size() - totalSent, 0);
        if (sent <= 0) {
            return false;
        }
        totalSent += sent;
    }

    return true;
}

void ClientSession::processData(const uint8_t* data, size_t length) {
    buffer_.append(data, length);

    // Process all complete messages in buffer
    while (buffer_.hasCompleteMessage()) {
        Protocol::Message msg = buffer_.extractMessage();
        handleMessage(msg);
    }
}

void ClientSession::handleMessage(const Protocol::Message& msg) {
    try {
        server_->log("[" + address_ + "] Received: " + Protocol::messageTypeToString(msg.type));

        switch (msg.type) {
            case Protocol::MessageType::REGISTER:
                handleRegister(msg);
                break;

            case Protocol::MessageType::LOGIN:
                handleLogin(msg);
                break;

            case Protocol::MessageType::LOGOUT:
                handleLogout(msg);
                break;

            case Protocol::MessageType::CHANGE_PASSWORD:
                handleChangePassword(msg);
                break;

            case Protocol::MessageType::MSG_GLOBAL:
                handleGlobalMessage(msg);
                break;

            case Protocol::MessageType::MSG_PRIVATE:
                handlePrivateMessage(msg);
                break;

            case Protocol::MessageType::PING:
                sendMessage(Protocol::Message(Protocol::MessageType::PONG));
                break;

            // Admin commands
            case Protocol::MessageType::KICK_USER:
                handleKickUser(msg);
                break;

            case Protocol::MessageType::BAN_USER:
                handleBanUser(msg);
                break;

            case Protocol::MessageType::UNBAN_USER:
                handleUnbanUser(msg);
                break;

            case Protocol::MessageType::MUTE_USER:
                handleMuteUser(msg);
                break;

            case Protocol::MessageType::UNMUTE_USER:
                handleUnmuteUser(msg);
                break;

            case Protocol::MessageType::PROMOTE_USER:
                handlePromoteUser(msg);
                break;

            case Protocol::MessageType::DEMOTE_USER:
                handleDemoteUser(msg);
                break;

            case Protocol::MessageType::GET_ALL_USERS:
                handleGetAllUsers(msg);
                break;

            case Protocol::MessageType::GET_BANNED_LIST:
                handleGetBannedList(msg);
                break;

            case Protocol::MessageType::GET_MUTED_LIST:
                handleGetMutedList(msg);
                break;

            case Protocol::MessageType::USER_INFO:
                handleUserInfo(msg);
                break;

            default:
                sendMessage(Protocol::createErrorResponse("Unknown command"));
                break;
        }
    } catch (const std::exception& e) {
        server_->log("[" + address_ + "] Error handling message: " + std::string(e.what()));
        try {
            sendMessage(Protocol::createErrorResponse("Internal server error"));
        } catch (...) {}
    } catch (...) {
        server_->log("[" + address_ + "] Unknown error handling message");
    }
}

void ClientSession::handleRegister(const Protocol::Message& msg) {
    // Parse credentials from content (JSON format: {"username": "...", "password": "..."})
    try {
        json j = json::parse(msg.content);
        std::string username = j["username"].get<std::string>();
        std::string password = j["password"].get<std::string>();

        // Validate input
        if (username.empty() || password.empty()) {
            sendMessage(Protocol::createErrorResponse("Username and password are required"));
            return;
        }

        if (username.length() < 3 || username.length() > 20) {
            sendMessage(Protocol::createErrorResponse("Username must be 3-20 characters"));
            return;
        }

        if (password.length() < 4) {
            sendMessage(Protocol::createErrorResponse("Password must be at least 4 characters"));
            return;
        }

        // Register user
        if (Database::getInstance().registerUser(username, password)) {
            server_->log("User registered: " + username);
            sendMessage(Protocol::createOkResponse("Registration successful"));
        } else {
            sendMessage(Protocol::createErrorResponse("Username already exists"));
        }
    } catch (const json::exception& e) {
        server_->log("JSON parse error in register: " + std::string(e.what()));
        try {
            sendMessage(Protocol::createErrorResponse("Invalid request format"));
        } catch (...) {}
    } catch (const std::exception& e) {
        server_->log("Error in register: " + std::string(e.what()));
        try {
            sendMessage(Protocol::createErrorResponse("Server error"));
        } catch (...) {}
    }
}

void ClientSession::handleLogin(const Protocol::Message& msg) {
    if (authenticated_) {
        sendMessage(Protocol::createErrorResponse("Already logged in"));
        return;
    }

    try {
        json j = json::parse(msg.content);
        std::string username = j["username"].get<std::string>();
        std::string password = j["password"].get<std::string>();

        // Check if already logged in elsewhere
        if (server_->isUserOnline(username)) {
            sendMessage(Protocol::createErrorResponse("User already logged in from another location"));
            return;
        }

        // Check if user is banned
        if (Database::getInstance().isBanned(username)) {
            sendMessage(Protocol::createErrorResponse("Your account has been banned"));
            return;
        }

        // Authenticate
        if (Database::getInstance().authenticateUser(username, password)) {
            std::string displayName = Database::getInstance().getDisplayName(username);
            setAuthenticated(username, displayName);
            server_->registerUser(username, this);

            server_->log("User logged in: " + username + " from " + address_);

            // Send success response with user info
            json response;
            response["username"] = username;
            response["displayName"] = displayName;
            response["role"] = Database::getInstance().getUserRole(username);
            response["isMuted"] = Database::getInstance().isMuted(username);
            sendMessage(Protocol::createOkResponse("Login successful", response.dump()));

            // Broadcast user online status to all clients
            server_->broadcast(Protocol::createUserStatusMessage(username, Protocol::UserStatus::ONLINE));

            // Send current online list to this client
            sendMessage(Protocol::createOnlineListMessage(server_->getOnlineUsers()));
        } else {
            sendMessage(Protocol::createErrorResponse("Invalid username or password"));
        }
    } catch (const json::exception& e) {
        server_->log("JSON parse error in login: " + std::string(e.what()));
        try {
            sendMessage(Protocol::createErrorResponse("Invalid request format"));
        } catch (...) {}
    } catch (const std::exception& e) {
        server_->log("Error in login: " + std::string(e.what()));
        try {
            sendMessage(Protocol::createErrorResponse("Server error"));
        } catch (...) {}
    }
}

void ClientSession::handleLogout(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Not logged in"));
        return;
    }

    std::string username = getUsername();
    server_->log("User logged out: " + username);

    // Broadcast user offline status
    server_->broadcast(Protocol::createUserStatusMessage(username, Protocol::UserStatus::OFFLINE), socketFd_);

    // Unregister and clear auth
    server_->unregisterUser(username);
    clearAuthentication();

    sendMessage(Protocol::createOkResponse("Logged out successfully"));
}

void ClientSession::handleChangePassword(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in to change password"));
        return;
    }

    try {
        json j = json::parse(msg.content);
        std::string oldPassword = j["oldPassword"].get<std::string>();
        std::string newPassword = j["newPassword"].get<std::string>();

        if (newPassword.length() < 4) {
            sendMessage(Protocol::createErrorResponse("New password must be at least 4 characters"));
            return;
        }

        std::string username = getUsername();
        if (Database::getInstance().changePassword(username, oldPassword, newPassword)) {
            server_->log("Password changed for: " + username);
            sendMessage(Protocol::createOkResponse("Password changed successfully"));
        } else {
            sendMessage(Protocol::createErrorResponse("Incorrect old password"));
        }
    } catch (const std::exception& e) {
        sendMessage(Protocol::createErrorResponse("Invalid request format"));
    }
}

void ClientSession::handleGlobalMessage(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in to send messages"));
        return;
    }

    std::string username = getUsername();

    // Check if user is muted
    if (Database::getInstance().isMuted(username)) {
        sendMessage(Protocol::createErrorResponse("You are muted and cannot send messages"));
        return;
    }

    // Check rate limit
    if (!checkRateLimit()) {
        sendMessage(Protocol::createErrorResponse("Rate limit exceeded. Please wait before sending more messages."));
        return;
    }

    std::string content = msg.content;

    if (content.empty()) {
        return;  // Ignore empty messages
    }

    server_->log("Global message from " + username + ": " + content.substr(0, 50) +
                 (content.length() > 50 ? "..." : ""));

    // Log message to database
    Database::getInstance().logMessage(username, "", content, "global");

    // Create and broadcast global message
    Protocol::Message globalMsg = Protocol::createGlobalMessage(username, content);
    server_->broadcast(globalMsg);
}

void ClientSession::handlePrivateMessage(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in to send messages"));
        return;
    }

    std::string sender = getUsername();

    // Check if user is muted
    if (Database::getInstance().isMuted(sender)) {
        sendMessage(Protocol::createErrorResponse("You are muted and cannot send messages"));
        return;
    }

    // Check rate limit
    if (!checkRateLimit()) {
        sendMessage(Protocol::createErrorResponse("Rate limit exceeded. Please wait before sending more messages."));
        return;
    }

    std::string receiver = msg.receiver;
    std::string content = msg.content;

    if (receiver.empty()) {
        sendMessage(Protocol::createErrorResponse("Receiver not specified"));
        return;
    }

    if (content.empty()) {
        return;  // Ignore empty messages
    }

    if (receiver == sender) {
        sendMessage(Protocol::createErrorResponse("Cannot send message to yourself"));
        return;
    }

    server_->log("Private message from " + sender + " to " + receiver + ": " +
                 content.substr(0, 50) + (content.length() > 50 ? "..." : ""));

    // Log message to database
    Database::getInstance().logMessage(sender, receiver, content, "private");

    // Create private message
    Protocol::Message privateMsg = Protocol::createPrivateMessage(sender, receiver, content);

    // Send to receiver
    if (!server_->sendToUser(receiver, privateMsg)) {
        sendMessage(Protocol::createErrorResponse("User not online: " + receiver));
        return;
    }

    // Also send copy to sender (for display)
    sendMessage(privateMsg);
}

// ========== Admin Commands ==========

bool ClientSession::isAdmin() const {
    std::string username = getUsername();
    return Database::getInstance().isAdmin(username);
}

bool ClientSession::checkRateLimit() {
    std::lock_guard<std::mutex> lock(rateLimitMutex_);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - rateLimitWindowStart_).count();

    if (elapsed >= RATE_LIMIT_WINDOW_SECONDS) {
        // Reset window
        rateLimitWindowStart_ = now;
        rateLimitMessageCount_ = 1;
        return true;
    }

    rateLimitMessageCount_++;
    if (rateLimitMessageCount_ > RATE_LIMIT_MAX_MESSAGES) {
        return false;  // Rate limit exceeded
    }

    return true;
}

void ClientSession::handleKickUser(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in"));
        return;
    }

    if (!isAdmin()) {
        sendMessage(Protocol::createErrorResponse("Admin privileges required"));
        return;
    }

    std::string targetUser = msg.receiver;
    if (targetUser.empty()) {
        sendMessage(Protocol::createErrorResponse("Target user not specified"));
        return;
    }

    if (targetUser == getUsername()) {
        sendMessage(Protocol::createErrorResponse("Cannot kick yourself"));
        return;
    }

    // Check if target is online
    if (!server_->isUserOnline(targetUser)) {
        sendMessage(Protocol::createErrorResponse("User not online: " + targetUser));
        return;
    }

    // Send kick notification to target
    Protocol::Message kickMsg(Protocol::MessageType::KICKED);
    kickMsg.content = "You have been kicked by " + getUsername();
    server_->sendToUser(targetUser, kickMsg);

    // Force disconnect
    server_->kickUser(targetUser);

    server_->log("User kicked: " + targetUser + " by " + getUsername());
    sendMessage(Protocol::createOkResponse("User kicked: " + targetUser));

    // Broadcast offline status
    server_->broadcast(Protocol::createUserStatusMessage(targetUser, Protocol::UserStatus::OFFLINE));
}

void ClientSession::handleBanUser(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in"));
        return;
    }

    if (!isAdmin()) {
        sendMessage(Protocol::createErrorResponse("Admin privileges required"));
        return;
    }

    std::string targetUser = msg.receiver;
    if (targetUser.empty()) {
        sendMessage(Protocol::createErrorResponse("Target user not specified"));
        return;
    }

    if (targetUser == getUsername()) {
        sendMessage(Protocol::createErrorResponse("Cannot ban yourself"));
        return;
    }

    // Check if target is admin
    if (Database::getInstance().isAdmin(targetUser)) {
        sendMessage(Protocol::createErrorResponse("Cannot ban an admin"));
        return;
    }

    if (!Database::getInstance().userExists(targetUser)) {
        sendMessage(Protocol::createErrorResponse("User not found: " + targetUser));
        return;
    }

    if (Database::getInstance().banUser(targetUser)) {
        server_->log("User banned: " + targetUser + " by " + getUsername());

        // If user is online, kick them
        if (server_->isUserOnline(targetUser)) {
            Protocol::Message banMsg(Protocol::MessageType::BANNED);
            banMsg.content = "You have been banned by " + getUsername();
            server_->sendToUser(targetUser, banMsg);
            server_->kickUser(targetUser);
            server_->broadcast(Protocol::createUserStatusMessage(targetUser, Protocol::UserStatus::OFFLINE));
        }

        sendMessage(Protocol::createOkResponse("User banned: " + targetUser));
    } else {
        sendMessage(Protocol::createErrorResponse("Failed to ban user"));
    }
}

void ClientSession::handleUnbanUser(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in"));
        return;
    }

    if (!isAdmin()) {
        sendMessage(Protocol::createErrorResponse("Admin privileges required"));
        return;
    }

    std::string targetUser = msg.receiver;
    if (targetUser.empty()) {
        sendMessage(Protocol::createErrorResponse("Target user not specified"));
        return;
    }

    if (!Database::getInstance().userExists(targetUser)) {
        sendMessage(Protocol::createErrorResponse("User not found: " + targetUser));
        return;
    }

    if (Database::getInstance().unbanUser(targetUser)) {
        server_->log("User unbanned: " + targetUser + " by " + getUsername());
        sendMessage(Protocol::createOkResponse("User unbanned: " + targetUser));
    } else {
        sendMessage(Protocol::createErrorResponse("Failed to unban user"));
    }
}

void ClientSession::handleMuteUser(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in"));
        return;
    }

    if (!isAdmin()) {
        sendMessage(Protocol::createErrorResponse("Admin privileges required"));
        return;
    }

    std::string targetUser = msg.receiver;
    if (targetUser.empty()) {
        sendMessage(Protocol::createErrorResponse("Target user not specified"));
        return;
    }

    if (targetUser == getUsername()) {
        sendMessage(Protocol::createErrorResponse("Cannot mute yourself"));
        return;
    }

    if (Database::getInstance().isAdmin(targetUser)) {
        sendMessage(Protocol::createErrorResponse("Cannot mute an admin"));
        return;
    }

    if (!Database::getInstance().userExists(targetUser)) {
        sendMessage(Protocol::createErrorResponse("User not found: " + targetUser));
        return;
    }

    if (Database::getInstance().muteUser(targetUser)) {
        server_->log("User muted: " + targetUser + " by " + getUsername());

        // Notify the muted user if online
        if (server_->isUserOnline(targetUser)) {
            Protocol::Message muteMsg(Protocol::MessageType::MUTED);
            muteMsg.content = "You have been muted by " + getUsername();
            server_->sendToUser(targetUser, muteMsg);
        }

        sendMessage(Protocol::createOkResponse("User muted: " + targetUser));
    } else {
        sendMessage(Protocol::createErrorResponse("Failed to mute user"));
    }
}

void ClientSession::handleUnmuteUser(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in"));
        return;
    }

    if (!isAdmin()) {
        sendMessage(Protocol::createErrorResponse("Admin privileges required"));
        return;
    }

    std::string targetUser = msg.receiver;
    if (targetUser.empty()) {
        sendMessage(Protocol::createErrorResponse("Target user not specified"));
        return;
    }

    if (!Database::getInstance().userExists(targetUser)) {
        sendMessage(Protocol::createErrorResponse("User not found: " + targetUser));
        return;
    }

    if (Database::getInstance().unmuteUser(targetUser)) {
        server_->log("User unmuted: " + targetUser + " by " + getUsername());

        // Notify the unmuted user if online
        if (server_->isUserOnline(targetUser)) {
            Protocol::Message unmuteMsg(Protocol::MessageType::UNMUTED);
            unmuteMsg.content = "You have been unmuted by " + getUsername();
            server_->sendToUser(targetUser, unmuteMsg);
        }

        sendMessage(Protocol::createOkResponse("User unmuted: " + targetUser));
    } else {
        sendMessage(Protocol::createErrorResponse("Failed to unmute user"));
    }
}

void ClientSession::handlePromoteUser(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in"));
        return;
    }

    if (!isAdmin()) {
        sendMessage(Protocol::createErrorResponse("Admin privileges required"));
        return;
    }

    std::string targetUser = msg.receiver;
    if (targetUser.empty()) {
        sendMessage(Protocol::createErrorResponse("Target user not specified"));
        return;
    }

    if (!Database::getInstance().userExists(targetUser)) {
        sendMessage(Protocol::createErrorResponse("User not found: " + targetUser));
        return;
    }

    if (Database::getInstance().isAdmin(targetUser)) {
        sendMessage(Protocol::createErrorResponse("User is already an admin"));
        return;
    }

    if (Database::getInstance().setUserRole(targetUser, 1)) {
        server_->log("User promoted to admin: " + targetUser + " by " + getUsername());
        sendMessage(Protocol::createOkResponse("User promoted to admin: " + targetUser));
    } else {
        sendMessage(Protocol::createErrorResponse("Failed to promote user"));
    }
}

void ClientSession::handleDemoteUser(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in"));
        return;
    }

    if (!isAdmin()) {
        sendMessage(Protocol::createErrorResponse("Admin privileges required"));
        return;
    }

    std::string targetUser = msg.receiver;
    if (targetUser.empty()) {
        sendMessage(Protocol::createErrorResponse("Target user not specified"));
        return;
    }

    if (targetUser == getUsername()) {
        sendMessage(Protocol::createErrorResponse("Cannot demote yourself"));
        return;
    }

    if (!Database::getInstance().userExists(targetUser)) {
        sendMessage(Protocol::createErrorResponse("User not found: " + targetUser));
        return;
    }

    if (!Database::getInstance().isAdmin(targetUser)) {
        sendMessage(Protocol::createErrorResponse("User is not an admin"));
        return;
    }

    if (Database::getInstance().setUserRole(targetUser, 0)) {
        server_->log("User demoted from admin: " + targetUser + " by " + getUsername());
        sendMessage(Protocol::createOkResponse("User demoted from admin: " + targetUser));
    } else {
        sendMessage(Protocol::createErrorResponse("Failed to demote user"));
    }
}

void ClientSession::handleGetAllUsers(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in"));
        return;
    }

    if (!isAdmin()) {
        sendMessage(Protocol::createErrorResponse("Admin privileges required"));
        return;
    }

    std::vector<UserInfo> users = Database::getInstance().getAllUsers();

    json j = json::array();
    for (const auto& user : users) {
        json userJson;
        userJson["username"] = user.username;
        userJson["displayName"] = user.displayName;
        userJson["role"] = user.role;
        userJson["isBanned"] = user.isBanned;
        userJson["isMuted"] = user.isMuted;
        userJson["createdAt"] = user.createdAt;
        userJson["isOnline"] = server_->isUserOnline(user.username);
        j.push_back(userJson);
    }

    Protocol::Message response(Protocol::MessageType::GET_ALL_USERS);
    response.extra = j.dump();
    sendMessage(response);
}

void ClientSession::handleGetBannedList(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in"));
        return;
    }

    if (!isAdmin()) {
        sendMessage(Protocol::createErrorResponse("Admin privileges required"));
        return;
    }

    std::vector<std::string> bannedUsers = Database::getInstance().getBannedUsers();

    json j = bannedUsers;
    Protocol::Message response(Protocol::MessageType::GET_BANNED_LIST);
    response.extra = j.dump();
    sendMessage(response);
}

void ClientSession::handleGetMutedList(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in"));
        return;
    }

    if (!isAdmin()) {
        sendMessage(Protocol::createErrorResponse("Admin privileges required"));
        return;
    }

    std::vector<std::string> mutedUsers = Database::getInstance().getMutedUsers();

    json j = mutedUsers;
    Protocol::Message response(Protocol::MessageType::GET_MUTED_LIST);
    response.extra = j.dump();
    sendMessage(response);
}

void ClientSession::handleUserInfo(const Protocol::Message& msg) {
    if (!authenticated_) {
        sendMessage(Protocol::createErrorResponse("Must be logged in"));
        return;
    }

    std::string targetUser = msg.receiver;
    if (targetUser.empty()) {
        targetUser = getUsername();  // Get own info
    }

    if (!Database::getInstance().userExists(targetUser)) {
        sendMessage(Protocol::createErrorResponse("User not found: " + targetUser));
        return;
    }

    UserInfo info = Database::getInstance().getUserInfo(targetUser);

    json j;
    j["username"] = info.username;
    j["displayName"] = info.displayName;
    j["role"] = info.role;
    j["isBanned"] = info.isBanned;
    j["isMuted"] = info.isMuted;
    j["createdAt"] = info.createdAt;
    j["isOnline"] = server_->isUserOnline(info.username);

    Protocol::Message response(Protocol::MessageType::USER_INFO);
    response.extra = j.dump();
    sendMessage(response);
}
