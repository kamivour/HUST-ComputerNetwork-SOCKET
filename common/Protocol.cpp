/**
 * @file Protocol.cpp
 * @brief Implementation của Protocol layer
 */

#include "Protocol.h"
#include "../thirdparty/json.hpp"
#include <cstring>
#include <iomanip>
#include <sstream>
#include <chrono>

using json = nlohmann::json;

namespace Protocol {

std::vector<uint8_t> serialize(const Message& msg) {
    // Create JSON object
    json j;
    j["type"] = static_cast<int>(msg.type);
    j["sender"] = msg.sender;
    j["receiver"] = msg.receiver;
    j["content"] = msg.content;
    j["timestamp"] = msg.timestamp.empty() ? getCurrentTimestamp() : msg.timestamp;
    j["extra"] = msg.extra;

    // Convert to string
    std::string payload = j.dump();

    // Create result with 4-byte length prefix (network byte order - big endian)
    uint32_t length = static_cast<uint32_t>(payload.size());
    std::vector<uint8_t> result(4 + length);

    // Write length in big-endian
    result[0] = (length >> 24) & 0xFF;
    result[1] = (length >> 16) & 0xFF;
    result[2] = (length >> 8) & 0xFF;
    result[3] = length & 0xFF;

    // Write payload
    std::memcpy(result.data() + 4, payload.data(), length);

    // Debug log (can be enabled for detailed protocol analysis)
    #ifdef PROTOCOL_DEBUG_LOG
    std::cout << "[PROTOCOL] Serialize: Type=" << messageTypeToString(msg.type) 
              << ", Length=" << length << " bytes" << std::endl;
    std::cout << "[PROTOCOL] JSON Payload: " << payload << std::endl;
    std::cout << "[PROTOCOL] Header bytes: [" 
              << std::hex << (int)result[0] << " " << (int)result[1] << " " 
              << (int)result[2] << " " << (int)result[3] << "]" << std::dec << std::endl;
    #endif

    return result;
}

Message deserialize(const uint8_t* data, size_t length) {
    Message msg;

    try {
        std::string payload(reinterpret_cast<const char*>(data), length);
        
        #ifdef PROTOCOL_DEBUG_LOG
        std::cout << "[PROTOCOL] Deserialize: Length=" << length << " bytes" << std::endl;
        std::cout << "[PROTOCOL] JSON Payload: " << payload << std::endl;
        #endif
        
        json j = json::parse(payload);

        msg.type = static_cast<MessageType>(j["type"].get<int>());
        msg.sender = j.value("sender", "");
        msg.receiver = j.value("receiver", "");
        msg.content = j.value("content", "");
        msg.timestamp = j.value("timestamp", "");
        msg.extra = j.value("extra", "");
        
        #ifdef PROTOCOL_DEBUG_LOG
        std::cout << "[PROTOCOL] Parsed: Type=" << messageTypeToString(msg.type) << std::endl;
        #endif
    } catch (const std::exception& e) {
        msg.type = MessageType::ERROR;
        msg.content = std::string("Parse error: ") + e.what();
        
        #ifdef PROTOCOL_DEBUG_LOG
        std::cerr << "[PROTOCOL] Parse error: " << e.what() << std::endl;
        #endif
    }

    return msg;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&time_t_now);

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << tm_now->tm_hour << ":"
        << std::setw(2) << tm_now->tm_min << ":"
        << std::setw(2) << tm_now->tm_sec;

    return oss.str();
}

std::string messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::REGISTER: return "REGISTER";
        case MessageType::LOGIN: return "LOGIN";
        case MessageType::LOGOUT: return "LOGOUT";
        case MessageType::CHANGE_PASSWORD: return "CHANGE_PASSWORD";
        case MessageType::MSG_GLOBAL: return "MSG_GLOBAL";
        case MessageType::MSG_PRIVATE: return "MSG_PRIVATE";
        case MessageType::ONLINE_LIST: return "ONLINE_LIST";
        case MessageType::USER_STATUS: return "USER_STATUS";
        case MessageType::USER_INFO: return "USER_INFO";
        case MessageType::KICK_USER: return "KICK_USER";
        case MessageType::BAN_USER: return "BAN_USER";
        case MessageType::UNBAN_USER: return "UNBAN_USER";
        case MessageType::MUTE_USER: return "MUTE_USER";
        case MessageType::UNMUTE_USER: return "UNMUTE_USER";
        case MessageType::PROMOTE_USER: return "PROMOTE_USER";
        case MessageType::DEMOTE_USER: return "DEMOTE_USER";
        case MessageType::GET_ALL_USERS: return "GET_ALL_USERS";
        case MessageType::GET_BANNED_LIST: return "GET_BANNED_LIST";
        case MessageType::GET_MUTED_LIST: return "GET_MUTED_LIST";
        case MessageType::KICKED: return "KICKED";
        case MessageType::BANNED: return "BANNED";
        case MessageType::MUTED: return "MUTED";
        case MessageType::UNMUTED: return "UNMUTED";
        case MessageType::OK: return "OK";
        case MessageType::ERROR: return "ERROR";
        case MessageType::PING: return "PING";
        case MessageType::PONG: return "PONG";
        default: return "UNKNOWN";
    }
}

Message createOkResponse(const std::string& content, const std::string& extra) {
    Message msg(MessageType::OK);
    msg.content = content;
    msg.extra = extra;
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

Message createErrorResponse(const std::string& content) {
    Message msg(MessageType::ERROR);
    msg.content = content;
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

Message createGlobalMessage(const std::string& sender, const std::string& content) {
    Message msg(MessageType::MSG_GLOBAL);
    msg.sender = sender;
    msg.content = content;
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

Message createPrivateMessage(const std::string& sender, const std::string& receiver, const std::string& content) {
    Message msg(MessageType::MSG_PRIVATE);
    msg.sender = sender;
    msg.receiver = receiver;
    msg.content = content;
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

Message createOnlineListMessage(const std::vector<std::string>& users) {
    Message msg(MessageType::ONLINE_LIST);
    json j = users;
    msg.extra = j.dump();
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

Message createUserStatusMessage(const std::string& username, UserStatus status) {
    Message msg(MessageType::USER_STATUS);
    msg.sender = username;
    msg.content = (status == UserStatus::ONLINE) ? "online" : "offline";
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

// MessageBuffer implementation
// Maximum message size: 1MB (to prevent memory issues with malformed data)
static const uint32_t MAX_MESSAGE_SIZE = 1024 * 1024;

MessageBuffer::MessageBuffer() {}

void MessageBuffer::append(const uint8_t* data, size_t length) {
    buffer_.insert(buffer_.end(), data, data + length);
}

bool MessageBuffer::hasCompleteMessage() const {
    // Need at least 4 bytes for length prefix
    if (buffer_.size() < 4) {
        return false;
    }

    // Read length (big-endian)
    uint32_t length = (static_cast<uint32_t>(buffer_[0]) << 24) |
                      (static_cast<uint32_t>(buffer_[1]) << 16) |
                      (static_cast<uint32_t>(buffer_[2]) << 8) |
                      static_cast<uint32_t>(buffer_[3]);

    // Validate message length to prevent overflow and memory issues
    if (length > MAX_MESSAGE_SIZE) {
        return false;  // Invalid message, will be handled in extractMessage
    }

    // Check if we have complete message
    return buffer_.size() >= (4 + static_cast<size_t>(length));
}

Message MessageBuffer::extractMessage() {
    // Need at least 4 bytes for length prefix
    if (buffer_.size() < 4) {
        return Message(MessageType::ERROR);
    }

    // Read length
    uint32_t length = (static_cast<uint32_t>(buffer_[0]) << 24) |
                      (static_cast<uint32_t>(buffer_[1]) << 16) |
                      (static_cast<uint32_t>(buffer_[2]) << 8) |
                      static_cast<uint32_t>(buffer_[3]);

    // Validate message length
    if (length > MAX_MESSAGE_SIZE) {
        // Invalid message - clear buffer and return error
        buffer_.clear();
        Message msg(MessageType::ERROR);
        msg.content = "Message too large or invalid";
        return msg;
    }

    // Check if we have the complete message
    if (buffer_.size() < 4 + static_cast<size_t>(length)) {
        return Message(MessageType::ERROR);
    }

    // Parse message
    Message msg = deserialize(buffer_.data() + 4, length);

    // Remove processed data from buffer
    buffer_.erase(buffer_.begin(), buffer_.begin() + 4 + static_cast<ptrdiff_t>(length));

    return msg;
}

void MessageBuffer::clear() {
    buffer_.clear();
}

} // namespace Protocol
