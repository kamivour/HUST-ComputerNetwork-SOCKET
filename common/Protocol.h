/**
 * @file Protocol.h
 * @brief Định nghĩa protocol cho giao tiếp client-server
 *
 * Protocol Format:
 * [4 bytes: message length][JSON payload]
 *
 * Message Types:
 * - REGISTER, LOGIN, LOGOUT, CHANGE_PASSWORD
 * - MSG_GLOBAL, MSG_PRIVATE
 * - ONLINE_LIST, USER_STATUS
 * - OK, ERROR
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

namespace Protocol {

// Message Types
enum class MessageType {
    // Authentication
    REGISTER = 1,
    LOGIN = 2,
    LOGOUT = 3,
    CHANGE_PASSWORD = 4,

    // Chat
    MSG_GLOBAL = 10,
    MSG_PRIVATE = 11,

    // User Management
    ONLINE_LIST = 20,
    USER_STATUS = 21,  // User online/offline notification
    USER_INFO = 22,    // Get user information

    // Member Management (Admin only)
    KICK_USER = 30,    // Kick user from server
    BAN_USER = 31,     // Ban user (cannot login)
    UNBAN_USER = 32,   // Unban user
    MUTE_USER = 33,    // Mute user (cannot send messages)
    UNMUTE_USER = 34,  // Unmute user
    PROMOTE_USER = 35, // Promote to admin
    DEMOTE_USER = 36,  // Demote to member
    GET_ALL_USERS = 37,// Get all registered users
    GET_BANNED_LIST = 38, // Get banned users list
    GET_MUTED_LIST = 39,  // Get muted users list

    // Notifications
    KICKED = 40,       // You have been kicked
    BANNED = 41,       // You have been banned
    MUTED = 42,        // You have been muted
    UNMUTED = 43,      // You have been unmuted

    // Responses
    OK = 100,
    ERROR = 101,

    // Heartbeat
    PING = 200,
    PONG = 201
};

// User Roles
enum class UserRole {
    MEMBER = 0,
    ADMIN = 1
};

// User Status
enum class UserStatus {
    ONLINE = 1,
    OFFLINE = 2
};

// Message structure
struct Message {
    MessageType type;
    std::string sender;
    std::string receiver;      // For private messages
    std::string content;
    std::string timestamp;
    std::string extra;         // Additional data (JSON format)

    Message() : type(MessageType::OK) {}
    Message(MessageType t) : type(t) {}
};

/**
 * @brief Serialize message to bytes with length prefix
 * @param msg Message to serialize
 * @return Vector of bytes ready to send
 */
std::vector<uint8_t> serialize(const Message& msg);

/**
 * @brief Deserialize bytes to message
 * @param data Pointer to data buffer (without length prefix)
 * @param length Length of data
 * @return Parsed message
 */
Message deserialize(const uint8_t* data, size_t length);

/**
 * @brief Get current timestamp as string (HH:MM:SS)
 * @return Formatted timestamp
 */
std::string getCurrentTimestamp();

/**
 * @brief Convert MessageType to string for logging
 * @param type MessageType enum
 * @return String representation
 */
std::string messageTypeToString(MessageType type);

/**
 * @brief Create OK response message
 * @param content Response content
 * @param extra Extra data (optional)
 * @return OK message
 */
Message createOkResponse(const std::string& content = "", const std::string& extra = "");

/**
 * @brief Create ERROR response message
 * @param content Error description
 * @return ERROR message
 */
Message createErrorResponse(const std::string& content);

/**
 * @brief Create global chat message
 * @param sender Sender username
 * @param content Message content
 * @return Global message
 */
Message createGlobalMessage(const std::string& sender, const std::string& content);

/**
 * @brief Create private chat message
 * @param sender Sender username
 * @param receiver Receiver username
 * @param content Message content
 * @return Private message
 */
Message createPrivateMessage(const std::string& sender, const std::string& receiver, const std::string& content);

/**
 * @brief Create online list message
 * @param users Vector of online usernames
 * @return Online list message
 */
Message createOnlineListMessage(const std::vector<std::string>& users);

/**
 * @brief Create user status message (online/offline notification)
 * @param username Username
 * @param status User status
 * @return User status message
 */
Message createUserStatusMessage(const std::string& username, UserStatus status);

// Buffer class for handling TCP stream fragmentation
class MessageBuffer {
public:
    MessageBuffer();

    /**
     * @brief Append received data to buffer
     * @param data Pointer to received data
     * @param length Length of received data
     */
    void append(const uint8_t* data, size_t length);

    /**
     * @brief Check if a complete message is available
     * @return true if complete message available
     */
    bool hasCompleteMessage() const;

    /**
     * @brief Extract next complete message from buffer
     * @return Parsed message (empty if no complete message)
     */
    Message extractMessage();

    /**
     * @brief Clear the buffer
     */
    void clear();

    /**
     * @brief Get current buffer size
     * @return Buffer size in bytes
     */
    size_t size() const { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;
};

} // namespace Protocol

#endif // PROTOCOL_H
