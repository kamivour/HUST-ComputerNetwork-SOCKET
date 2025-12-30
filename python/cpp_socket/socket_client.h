/**
 * @file socket_client.h
 * @brief C++ Socket Client Library - DLL Export Header
 *
 * This library provides Winsock socket functionality that can be called from Python via ctypes.
 */

#ifndef SOCKET_CLIENT_H
#define SOCKET_CLIENT_H

#ifdef _WIN32
    #ifdef SOCKET_CLIENT_EXPORTS
        #define SOCKET_API __declspec(dllexport)
    #else
        #define SOCKET_API __declspec(dllimport)
    #endif
#else
    #define SOCKET_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Winsock library
 * @return 0 on success, -1 on failure
 */
SOCKET_API int socket_init(void);

/**
 * @brief Cleanup Winsock library
 */
SOCKET_API void socket_cleanup(void);

/**
 * @brief Connect to server
 * @param host Server hostname or IP address
 * @param port Server port
 * @return 0 on success, -1 on failure
 */
SOCKET_API int socket_connect(const char* host, int port);

/**
 * @brief Disconnect from server
 */
SOCKET_API void socket_disconnect(void);

/**
 * @brief Check if connected
 * @return 1 if connected, 0 if not
 */
SOCKET_API int socket_is_connected(void);

/**
 * @brief Send raw data to server
 * @param data Pointer to data buffer
 * @param length Length of data
 * @return Number of bytes sent, -1 on error
 */
SOCKET_API int socket_send_raw(const char* data, int length);

/**
 * @brief Receive raw data from server (non-blocking check)
 * @param buffer Buffer to store received data
 * @param max_length Maximum length to receive
 * @return Number of bytes received, 0 if no data, -1 on error/disconnect
 */
SOCKET_API int socket_recv_raw(char* buffer, int max_length);

/**
 * @brief Send a protocol message (with length prefix)
 * @param json_data JSON string to send
 * @return 0 on success, -1 on failure
 */
SOCKET_API int socket_send_message(const char* json_data);

/**
 * @brief Receive a complete protocol message
 * @param buffer Buffer to store JSON message
 * @param max_length Maximum buffer length
 * @return Length of message received, 0 if no complete message, -1 on error
 */
SOCKET_API int socket_recv_message(char* buffer, int max_length);

/**
 * @brief Get last error message
 * @return Error message string
 */
SOCKET_API const char* socket_get_error(void);

/**
 * @brief Set socket to non-blocking mode
 * @param non_blocking 1 for non-blocking, 0 for blocking
 * @return 0 on success, -1 on failure
 */
SOCKET_API int socket_set_nonblocking(int non_blocking);

#ifdef __cplusplus
}
#endif

#endif // SOCKET_CLIENT_H
