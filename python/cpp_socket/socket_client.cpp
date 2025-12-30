/**
 * @file socket_client.cpp
 * @brief C++ Socket Client Library - Winsock Implementation
 *
 * This is the core socket code using Winsock2.
 * Compiled as DLL to be called from Python via ctypes.
 */

#define SOCKET_CLIENT_EXPORTS

#include "socket_client.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

// ==================== Global State ====================

static SOCKET g_socket = INVALID_SOCKET;
static bool g_initialized = false;
static bool g_connected = false;
static std::string g_last_error;
static std::mutex g_mutex;

// Message buffer for handling TCP stream
static std::vector<char> g_recv_buffer;
static std::mutex g_buffer_mutex;

// ==================== Helper Functions ====================

static void set_error(const std::string& error) {
    g_last_error = error;
}

static void set_winsock_error(const std::string& prefix) {
    int err = WSAGetLastError();
    g_last_error = prefix + " (WSA Error: " + std::to_string(err) + ")";
}

// ==================== DLL Entry Point ====================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            // DLL loaded
            break;
        case DLL_PROCESS_DETACH:
            // DLL unloaded - cleanup
            socket_disconnect();
            socket_cleanup();
            break;
    }
    return TRUE;
}

// ==================== API Implementation ====================

SOCKET_API int socket_init(void) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_initialized) {
        return 0;  // Already initialized
    }

    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        set_error("WSAStartup failed with error: " + std::to_string(result));
        return -1;
    }

    g_initialized = true;
    return 0;
}

SOCKET_API void socket_cleanup(void) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_initialized) {
        WSACleanup();
        g_initialized = false;
    }
}

SOCKET_API int socket_connect(const char* host, int port) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_initialized) {
        set_error("Socket not initialized. Call socket_init() first.");
        return -1;
    }

    if (g_connected) {
        set_error("Already connected. Disconnect first.");
        return -1;
    }

    // Create socket
    g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_socket == INVALID_SOCKET) {
        set_winsock_error("Failed to create socket");
        return -1;
    }

    // Resolve hostname
    struct addrinfo hints, *result = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string port_str = std::to_string(port);
    int res = getaddrinfo(host, port_str.c_str(), &hints, &result);
    if (res != 0) {
        set_error("Failed to resolve hostname: " + std::string(host));
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        return -1;
    }

    // Connect
    res = connect(g_socket, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);

    if (res == SOCKET_ERROR) {
        set_winsock_error("Failed to connect to " + std::string(host) + ":" + std::to_string(port));
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        return -1;
    }

    g_connected = true;

    // Clear receive buffer
    {
        std::lock_guard<std::mutex> buf_lock(g_buffer_mutex);
        g_recv_buffer.clear();
    }

    return 0;
}

SOCKET_API void socket_disconnect(void) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_socket != INVALID_SOCKET) {
        shutdown(g_socket, SD_BOTH);
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
    }
    g_connected = false;

    // Clear buffer
    {
        std::lock_guard<std::mutex> buf_lock(g_buffer_mutex);
        g_recv_buffer.clear();
    }
}

SOCKET_API int socket_is_connected(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_connected ? 1 : 0;
}

SOCKET_API int socket_send_raw(const char* data, int length) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_connected || g_socket == INVALID_SOCKET) {
        set_error("Not connected");
        return -1;
    }

    int total_sent = 0;
    while (total_sent < length) {
        int sent = send(g_socket, data + total_sent, length - total_sent, 0);
        if (sent == SOCKET_ERROR) {
            set_winsock_error("Send failed");
            g_connected = false;
            return -1;
        }
        total_sent += sent;
    }

    return total_sent;
}

SOCKET_API int socket_recv_raw(char* buffer, int max_length) {
    // Don't lock g_mutex here - we want non-blocking behavior
    if (!g_connected || g_socket == INVALID_SOCKET) {
        return -1;
    }

    // Use select to check if data is available (non-blocking)
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(g_socket, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;  // Immediate return

    int result = select(0, &read_fds, nullptr, nullptr, &timeout);
    if (result == 0) {
        return 0;  // No data available
    }
    if (result == SOCKET_ERROR) {
        set_winsock_error("Select failed");
        return -1;
    }

    // Data available - receive it
    int received = recv(g_socket, buffer, max_length, 0);
    if (received == 0) {
        // Connection closed gracefully
        g_connected = false;
        return -1;
    }
    if (received == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return 0;  // No data
        }
        set_winsock_error("Recv failed");
        g_connected = false;
        return -1;
    }

    return received;
}

SOCKET_API int socket_send_message(const char* json_data) {
    if (!json_data) {
        set_error("Null data");
        return -1;
    }

    int json_length = (int)strlen(json_data);

    // Create message with 4-byte length prefix (big-endian)
    std::vector<char> message(4 + json_length);

    // Write length in big-endian
    message[0] = (json_length >> 24) & 0xFF;
    message[1] = (json_length >> 16) & 0xFF;
    message[2] = (json_length >> 8) & 0xFF;
    message[3] = json_length & 0xFF;

    // Copy JSON data
    memcpy(message.data() + 4, json_data, json_length);

    // Send
    int result = socket_send_raw(message.data(), (int)message.size());
    return (result > 0) ? 0 : -1;
}

SOCKET_API int socket_recv_message(char* buffer, int max_length) {
    // First, try to receive more data into our buffer
    char temp_buffer[4096];
    int received = socket_recv_raw(temp_buffer, sizeof(temp_buffer));

    if (received > 0) {
        std::lock_guard<std::mutex> lock(g_buffer_mutex);
        g_recv_buffer.insert(g_recv_buffer.end(), temp_buffer, temp_buffer + received);
    } else if (received < 0) {
        return -1;  // Error or disconnect
    }

    // Check if we have a complete message
    std::lock_guard<std::mutex> lock(g_buffer_mutex);

    if (g_recv_buffer.size() < 4) {
        return 0;  // Not enough data for header
    }

    // Read length from header (big-endian)
    uint32_t msg_length =
        ((uint8_t)g_recv_buffer[0] << 24) |
        ((uint8_t)g_recv_buffer[1] << 16) |
        ((uint8_t)g_recv_buffer[2] << 8) |
        ((uint8_t)g_recv_buffer[3]);

    if (g_recv_buffer.size() < 4 + msg_length) {
        return 0;  // Message not complete yet
    }

    // Check buffer size
    if ((int)msg_length >= max_length) {
        set_error("Message too large for buffer");
        return -1;
    }

    // Copy message to output buffer
    memcpy(buffer, g_recv_buffer.data() + 4, msg_length);
    buffer[msg_length] = '\0';  // Null-terminate

    // Remove processed message from buffer
    g_recv_buffer.erase(g_recv_buffer.begin(), g_recv_buffer.begin() + 4 + msg_length);

    return (int)msg_length;
}

SOCKET_API const char* socket_get_error(void) {
    return g_last_error.c_str();
}

SOCKET_API int socket_set_nonblocking(int non_blocking) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_socket == INVALID_SOCKET) {
        set_error("No socket");
        return -1;
    }

    u_long mode = non_blocking ? 1 : 0;
    int result = ioctlsocket(g_socket, FIONBIO, &mode);
    if (result != 0) {
        set_winsock_error("Failed to set non-blocking mode");
        return -1;
    }

    return 0;
}
