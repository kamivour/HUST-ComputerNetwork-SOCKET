/**
 * @file Server.h
 * @brief TCP Chat Server class
 */

#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>
#include "../common/Protocol.h"

// Structure for client info (GUI display)
struct ClientInfo {
    std::string username;
    std::string displayName;
    std::string address;
    int role;  // 0 = member, 1 = admin
    bool isAuthenticated;
};

class ClientSession;

class Server {
public:
    /**
     * @brief Construct a new Server
     * @param port Server port
     * @param maxClients Maximum number of clients (default 100)
     */
    Server(int port, int maxClients = 100);

    /**
     * @brief Destroy the Server
     */
    ~Server();

    // Disable copy
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    /**
     * @brief Start the server
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the server
     */
    void stop();

    /**
     * @brief Check if server is running
     * @return true if running
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Get server port
     * @return Port number
     */
    int getPort() const { return port_; }

    /**
     * @brief Get number of connected clients
     * @return Client count
     */
    size_t getClientCount() const;

    /**
     * @brief Get list of online usernames
     * @return Vector of usernames
     */
    std::vector<std::string> getOnlineUsers() const;

    /**
     * @brief Check if a username is online
     * @param username Username to check
     * @return true if online
     */
    bool isUserOnline(const std::string& username) const;

    /**
     * @brief Get list of all connected clients with their info
     * @return Vector of ClientInfo structures
     */
    std::vector<ClientInfo> getConnectedClients() const;

    /**
     * @brief Broadcast a server message to all clients
     * @param message Message content
     */
    void broadcastServerMessage(const std::string& message);

    /**
     * @brief Send a server message to a specific user
     * @param username Target username
     * @param message Message content
     * @return true if sent successfully
     */
    bool sendServerMessageToUser(const std::string& username, const std::string& message);

    /**
     * @brief Broadcast message to all authenticated clients
     * @param msg Message to broadcast
     * @param excludeSocket Socket to exclude (optional, -1 for none)
     */
    void broadcast(const Protocol::Message& msg, int excludeSocket = -1);

    /**
     * @brief Send message to a specific user
     * @param username Target username
     * @param msg Message to send
     * @return true if user found and message sent
     */
    bool sendToUser(const std::string& username, const Protocol::Message& msg);

    /**
     * @brief Register a client session after login
     * @param username Username
     * @param session Client session pointer
     */
    void registerUser(const std::string& username, ClientSession* session);

    /**
     * @brief Unregister a user (on logout/disconnect)
     * @param username Username
     */
    void unregisterUser(const std::string& username);

    /**
     * @brief Force disconnect a user (kick)
     * @param username Username to kick
     */
    void kickUser(const std::string& username);

    /**
     * @brief Log server event
     * @param event Event description
     */
    void log(const std::string& event);

private:
    /**
     * @brief Main accept loop
     */
    void acceptLoop();

    /**
     * @brief Handle a client connection
     * @param socketFd Client socket
     * @param address Client address
     */
    void handleClient(int socketFd, const std::string& address);

    /**
     * @brief Remove a client session
     * @param socketFd Socket fd to remove
     */
    void removeClient(int socketFd);

    /**
     * @brief Broadcast online list to all clients
     */
    void broadcastOnlineList();

    int port_;
    int maxClients_;
    int serverSocket_;
    std::atomic<bool> running_;

    // Active client sessions (socket -> session)
    std::map<int, std::unique_ptr<ClientSession>> clients_;
    mutable std::mutex clientsMutex_;

    // Username to socket mapping for quick lookup
    std::map<std::string, int> userToSocket_;
    mutable std::mutex userMapMutex_;

    // Accept thread
    std::thread acceptThread_;

    // Note: Client handler threads are detached to avoid memory accumulation
    // They will clean up on their own when the connection closes
};

#endif // SERVER_H
