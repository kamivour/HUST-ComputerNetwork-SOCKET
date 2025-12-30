# TCP Socket Chat Application - Project Overview

**Date:** December 30, 2025  
**Project:** Multi-threaded TCP Socket Chat Server & Qt Client  
**Language:** C++17

---

## Table of Contents
1. [Project Architecture](#project-architecture)
2. [Core Components](#core-components)
3. [Socket Implementation Deep Dive](#socket-implementation-deep-dive)
4. [Communication Protocol](#communication-protocol)
5. [Key Features](#key-features)
6. [Security Features](#security-features)

---

## Project Architecture

### Overall Structure
This is a **client-server TCP socket chat application** built with C++17, featuring a Qt-based GUI client and a multi-threaded server with SQLite database integration.

### Directory Structure
```
mmt_c/
├── CMakeLists.txt          # Build configuration
├── README.md               # Project documentation
├── common/                 # Shared protocol layer
│   ├── Protocol.cpp
│   └── Protocol.h
├── server/                 # Server implementation
│   ├── Server.cpp
│   ├── Server.h
│   ├── ClientSession.cpp
│   ├── ClientSession.h
│   ├── Database.cpp
│   ├── Database.h
│   └── server_main.cpp
├── client/                 # Client implementation (Qt)
│   ├── ChatClient.cpp
│   ├── ChatClient.h
│   ├── MainWindow.cpp
│   ├── MainWindow.h
│   ├── AuthDialog.cpp
│   ├── AuthDialog.h
│   └── client_main.cpp
└── thirdparty/
    └── json.hpp           # nlohmann/json library
```

---

## Core Components

### 1. Common Layer (`common/`)

**Protocol.h/cpp** - Communication Protocol Implementation

#### Message Format
```
┌────────────┬─────────────────────────────┐
│ 4 bytes    │ N bytes                     │
│ Length (N) │ JSON Payload                │
└────────────┴─────────────────────────────┘
```

#### Message Types
- **Authentication:**
  - `REGISTER` - User registration
  - `LOGIN` - User login
  - `LOGOUT` - User logout
  - `CHANGE_PASSWORD` - Password change

- **Chat:**
  - `MSG_GLOBAL` - Broadcast message to all users
  - `MSG_PRIVATE` - Direct 1-on-1 message

- **User Management:**
  - `ONLINE_LIST` - List of online users
  - `USER_STATUS` - User online/offline notification
  - `USER_INFO` - Get user information

- **Admin Commands:**
  - `KICK_USER` - Kick user from server
  - `BAN_USER` - Ban user (cannot login)
  - `UNBAN_USER` - Unban user
  - `MUTE_USER` - Mute user (cannot send messages)
  - `UNMUTE_USER` - Unmute user
  - `PROMOTE_USER` - Promote to admin
  - `DEMOTE_USER` - Demote to member
  - `GET_ALL_USERS` - Get all registered users
  - `GET_BANNED_LIST` - Get banned users list
  - `GET_MUTED_LIST` - Get muted users list

- **Notifications:**
  - `KICKED` - You have been kicked
  - `BANNED` - You have been banned
  - `MUTED` - You have been muted
  - `UNMUTED` - You have been unmuted

- **Responses:**
  - `OK` - Success response
  - `ERROR` - Error response

- **Heartbeat:**
  - `PING` - Heartbeat request
  - `PONG` - Heartbeat response

#### Key Classes

**Message Structure:**
```cpp
struct Message {
    MessageType type;
    std::string sender;
    std::string receiver;
    std::string content;
    std::string timestamp;
    std::string extra;
};
```

**MessageBuffer Class:**
Handles TCP stream fragmentation by accumulating data and extracting complete messages based on length prefix.

---

### 2. Server Layer (`server/`)

#### Server.h/cpp - Main Server Orchestrator

**Responsibilities:**
- Multi-threaded TCP server (supports up to 100 concurrent clients)
- Client connection management
- Message broadcasting and routing
- User session tracking

**Key Methods:**
- `start()` - Initialize and start server
- `stop()` - Gracefully shutdown server
- `acceptLoop()` - Accept incoming connections (blocking)
- `handleClient()` - Per-client message handling thread
- `broadcast()` - Send message to all authenticated clients
- `sendToUser()` - Send message to specific user

#### ClientSession.h/cpp - Individual Client Handler

**Responsibilities:**
- Manages one client connection per instance
- Handles authentication state
- Message sending/receiving
- Request processing and routing

**Key Methods:**
- `sendMessage()` - Send protocol message to client
- `processData()` - Process received raw bytes
- `handleMessage()` - Route message to appropriate handler
- Individual handlers: `handleLogin()`, `handleRegister()`, `handleGlobalMessage()`, etc.

#### Database.h/cpp - SQLite Data Persistence

**Responsibilities:**
- User registration with password hashing
- Authentication
- Password changes
- Role management (Member/Admin)
- Moderation tracking (Ban/Mute)

**Key Methods:**
- `registerUser()` - Create new user account
- `authenticateUser()` - Verify login credentials
- `changePassword()` - Update user password
- `isBanned()`, `isMuted()` - Check user status
- `isAdmin()` - Check admin privileges
- `banUser()`, `muteUser()` - Apply restrictions
- `promoteToAdmin()`, `demoteToMember()` - Manage roles

**Database Schema:**
- Thread-safe singleton pattern
- SQLite3 backend
- Tables: users (with password hashing, roles, ban/mute status)

---

### 3. Client Layer (`client/`)

#### ChatClient.h/cpp - Network Layer (Qt-based)

**Responsibilities:**
- Qt TCP socket integration (`QTcpSocket`)
- Asynchronous message handling with signals/slots
- Connection state management
- Command interface for all server operations

**Connection States:**
1. `Disconnected` - Not connected
2. `Connecting` - Connection in progress
3. `Connected` - TCP connected but not authenticated
4. `Authenticated` - Logged in and ready to chat

**Key Features:**
- Non-blocking I/O through Qt event loop
- Signal-driven architecture
- Automatic reconnection support
- Heartbeat mechanism (PING/PONG every 30 seconds)

#### MainWindow.h/cpp - Main GUI

**Features:**
- Tabbed chat interface (global chat + private chats)
- Online user list with real-time updates
- Admin panel for user management
- Status indicators (connection, mute status)
- Message input with send button
- User info display

**Key Components:**
- `ChatTab` - Individual chat tab widget
- `ChangePasswordDialog` - Password change form
- Main window with QTabWidget for multiple conversations

#### AuthDialog.h/cpp - Login/Register Dialog

**Features:**
- User authentication interface
- Registration form with validation
- Switch between login/register modes
- Input validation (username length, password strength)

---

## Socket Implementation Deep Dive

### 1. Socket Type & Protocol

#### Socket Configuration
- **Socket Family:** `AF_INET` (IPv4)
- **Socket Type:** `SOCK_STREAM` (TCP)
- **Protocol:** TCP (Transmission Control Protocol)

#### TCP Characteristics
- **Connection-oriented:** Reliable, ordered, error-checked delivery
- **Stream-based:** Data sent as continuous byte stream
- **Full-duplex:** Simultaneous bidirectional communication
- **Flow control:** Built-in congestion management

---

### 2. Server vs Client: Different Socket Implementations

| Aspect | Server | Client |
|--------|--------|--------|
| **Library** | Raw WinSock2/POSIX | Qt's QTcpSocket |
| **API Style** | Manual C-style socket calls | Object-oriented Qt wrapper |
| **Platform Handling** | Explicit `#ifdef _WIN32` | Qt handles cross-platform |
| **Async Model** | Manual threading | Qt event loop + signals/slots |
| **Initialization** | `WSAStartup()` (Windows) | Automatic by Qt |
| **Socket Functions** | socket(), bind(), listen(), accept() | connectToHost() |
| **I/O Model** | Blocking recv() in threads | Event-driven readyRead signal |

---

### 3. Server Socket Lifecycle (Step-by-Step)

#### Initialization & Setup

```cpp
// STEP 1: INITIALIZATION (Windows-specific)
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);  // Initialize WinSock 2.2
#endif

// STEP 2: CREATE SOCKET
serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
// Creates TCP socket, returns file descriptor (or -1 on error)
// AF_INET = IPv4, SOCK_STREAM = TCP, 0 = default protocol

// STEP 3: SET SOCKET OPTIONS
int opt = 1;
setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
// SO_REUSEADDR: Allows immediate port reuse after server restart
// Prevents "Address already in use" errors

// STEP 4: BIND TO ADDRESS
struct sockaddr_in serverAddr;
memset(&serverAddr, 0, sizeof(serverAddr));
serverAddr.sin_family = AF_INET;
serverAddr.sin_addr.s_addr = INADDR_ANY;  // Listen on all network interfaces (0.0.0.0)
serverAddr.sin_port = htons(port_);        // Convert port to network byte order (big-endian)

bind(serverSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
// Associates socket with specific IP address and port
// Example: binds to 0.0.0.0:8080

// STEP 5: LISTEN FOR CONNECTIONS
listen(serverSocket_, maxClients_);
// Marks socket as passive (listening) socket
// maxClients_ = backlog (max pending connections in queue)
// Transitions socket to LISTENING state

// STEP 6: ACCEPT LOOP (in separate thread)
void Server::acceptLoop() {
    while (running_) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        // BLOCKING CALL - waits for incoming connection
        int clientSocket = accept(serverSocket_, 
                                  (struct sockaddr*)&clientAddr, 
                                  &clientLen);
        
        // Returns NEW socket FD for the accepted client
        // Original serverSocket_ continues listening
        
        // Extract client IP address
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, INET_ADDRSTRLEN);
        
        // Create ClientSession and spawn handler thread
        clients_[clientSocket] = std::make_unique<ClientSession>(clientSocket, this);
        clientThreads_.emplace_back(&Server::handleClient, this, clientSocket);
    }
}
```

#### Key Socket Functions Explained

1. **`socket()`** - Creates endpoint for communication
   - Returns: File descriptor (integer) representing the socket
   - On error: Returns -1

2. **`setsockopt()`** - Configures socket behavior
   - `SO_REUSEADDR`: Critical for development (immediate restart)
   - Other options: `SO_KEEPALIVE`, `TCP_NODELAY`, etc.

3. **`bind()`** - Associates socket with address
   - Binds to IP:PORT combination
   - `INADDR_ANY` (0.0.0.0) = all network interfaces
   - Port must be available (not in use)

4. **`listen()`** - Marks socket as passive
   - Backlog parameter = max queued connections
   - Socket can now accept incoming connections

5. **`accept()`** - Accepts pending connection
   - **Blocking call** - waits for client
   - Returns **new socket** for the client
   - Original socket continues listening

---

### 4. Server Accept & Threading Model

#### Architecture Overview

```
┌─────────────────────────────────────────────┐
│           Main Server Thread                │
│                                             │
│  ┌─────────────────────────────────┐       │
│  │    acceptLoop()                 │       │
│  │    (blocks on accept())         │       │
│  └─────────────┬───────────────────┘       │
│                │                            │
│                │ New client connects        │
│                ↓                            │
│  ┌─────────────────────────────────┐       │
│  │  Create ClientSession           │       │
│  │  Spawn client handler thread    │       │
│  └─────────────┬───────────────────┘       │
└────────────────┼────────────────────────────┘
                 │
    ┌────────────┴────────────┬──────────────┐
    ↓                         ↓              ↓
┌─────────┐             ┌─────────┐     ┌─────────┐
│ Client  │             │ Client  │     │ Client  │
│ Thread  │             │ Thread  │     │ Thread  │
│   #1    │             │   #2    │     │   #N    │
│         │             │         │     │         │
│ recv()  │             │ recv()  │     │ recv()  │
│ loop    │             │ loop    │     │ loop    │
└─────────┘             └─────────┘     └─────────┘
```

#### Per-Client Handler Thread

```cpp
void Server::handleClient(int socketFd, const std::string& address) {
    uint8_t buffer[BUFFER_SIZE];  // 4096 bytes
    ClientSession* session = clients_[socketFd].get();
    
    try {
        while (running_ && session->isActive()) {
            // BLOCKING READ - waits for data
            ssize_t bytesRead = recv(socketFd, 
                                    reinterpret_cast<char*>(buffer), 
                                    BUFFER_SIZE, 
                                    0);
            
            if (bytesRead <= 0) {
                // bytesRead == 0: Client disconnected gracefully
                // bytesRead < 0: Error occurred
                break;
            }
            
            // Process received data
            session->processData(buffer, bytesRead);
        }
    } catch (const std::exception& e) {
        log("Exception in client handler: " + std::string(e.what()));
    }
    
    // Cleanup on disconnect
    unregisterUser(session->getUsername());
    
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.erase(socketFd);  // Closes socket in destructor
    }
}
```

#### Thread Synchronization

**Mutexes Used:**
- `clientsMutex_` - Protects `clients_` map
- `usersMutex_` - Protects username-to-session mapping
- `threadsMutex_` - Protects thread vector
- `sendMutex_` (per session) - Protects socket send operations

**Thread Model:**
- **Main thread:** Accept loop (1 thread)
- **Per-client threads:** One thread per connected client
- **No thread pool:** Simple but scales to ~100 clients

---

### 5. Client Socket Lifecycle (Qt Abstraction)

#### Qt Socket Architecture

```cpp
// CONSTRUCTOR: Create QTcpSocket
ChatClient::ChatClient(QObject* parent)
    : QObject(parent)
    , socket_(new QTcpSocket(this))  // Qt manages underlying socket
{
    // SIGNAL/SLOT CONNECTIONS (event-driven)
    connect(socket_, &QTcpSocket::connected, 
            this, &ChatClient::onConnected);
    
    connect(socket_, &QTcpSocket::disconnected, 
            this, &ChatClient::onDisconnected);
    
    connect(socket_, &QTcpSocket::readyRead, 
            this, &ChatClient::onReadyRead);
    
    connect(socket_, 
            QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &ChatClient::onSocketError);
}

// INITIATE CONNECTION (non-blocking)
void ChatClient::connectToServer(const QString& host, quint16 port) {
    setState(ConnectionState::Connecting);
    socket_->connectToHost(host, port);  // Asynchronous - returns immediately
    // Qt will emit 'connected' signal when ready
}

// CONNECTION ESTABLISHED (signal callback)
void ChatClient::onConnected() {
    setState(ConnectionState::Connected);
    pingTimer_->start();  // Start heartbeat
    emit connected();     // Notify UI
}

// DATA AVAILABLE (event-driven)
void ChatClient::onReadyRead() {
    // Qt has buffered received data internally
    QByteArray data = socket_->readAll();
    
    // Accumulate in our protocol buffer
    buffer_.append(reinterpret_cast<const uint8_t*>(data.constData()), 
                   data.size());
    
    // Extract all complete messages
    while (buffer_.hasCompleteMessage()) {
        Protocol::Message msg = buffer_.extractMessage();
        processMessage(msg);
    }
}

// DISCONNECTION (signal callback)
void ChatClient::onDisconnected() {
    pingTimer_->stop();
    buffer_.clear();
    setState(ConnectionState::Disconnected);
    emit disconnected();  // Notify UI
}
```

#### Qt Event Loop Model

```
┌─────────────────────────────────────────┐
│         Qt Event Loop                   │
│                                         │
│  ┌───────────────────────────────────┐ │
│  │   Socket Events Monitor           │ │
│  │   (select/epoll/IOCP internally)  │ │
│  └──────────┬────────────────────────┘ │
│             │                           │
│             │ Data available            │
│             ↓                           │
│  ┌───────────────────────────────────┐ │
│  │   Emit: readyRead() signal        │ │
│  └──────────┬────────────────────────┘ │
│             │                           │
│             ↓                           │
│  ┌───────────────────────────────────┐ │
│  │   Call: onReadyRead() slot        │ │
│  └───────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

#### Key Differences from Server

**No Low-Level Socket Operations:**
- ❌ No `socket()`, `bind()`, `listen()`, `accept()`
- ✅ Just `connectToHost(host, port)`

**Event-Driven (Non-Blocking):**
- Server: Blocking `recv()` in loop
- Client: Qt event loop + signals when data arrives

**Cross-Platform Automatic:**
- Qt handles Windows/Linux/macOS differences
- No `#ifdef _WIN32` needed
- No manual `WSAStartup()` / `WSACleanup()`

---

### 6. The TCP Stream Problem & Solution

#### The Problem: TCP is Stream-Oriented

TCP provides a **byte stream**, not discrete messages. Data boundaries are not preserved:

```
Application sends:
┌──────────────────┬──────────────┐
│ Message 1 (100B) │ Message 2 (50B) │
└──────────────────┴──────────────┘

TCP might deliver as:
┌────────┬──────────────────┬─────────────┐
│ 50B    │ 90B              │ 10B         │
└────────┴──────────────────┴─────────────┘
 Chunk 1      Chunk 2           Chunk 3

Or even:
┌────────────────────────────────────────┐
│ All 150 bytes in one recv()            │
└────────────────────────────────────────┘
```

**Consequences:**
- One `send()` ≠ one `recv()`
- Messages can split across multiple `recv()` calls
- Multiple messages can arrive in single `recv()`

---

#### The Solution: Length-Prefixed Protocol

**Wire Protocol Format:**
```
┌────────────────────┬─────────────────────────────────────┐
│   4 bytes          │        N bytes                      │
│   Length (N)       │        JSON Payload                 │
│   (Big-Endian)     │        {"type":1,"sender":"..."...} │
└────────────────────┴─────────────────────────────────────┘
```

#### Serialization (Message → Bytes)

```cpp
std::vector<uint8_t> Protocol::serialize(const Message& msg) {
    // 1. Convert message struct to JSON
    json j = {
        {"type", static_cast<int>(msg.type)},
        {"sender", msg.sender},
        {"receiver", msg.receiver},
        {"content", msg.content},
        {"timestamp", msg.timestamp},
        {"extra", msg.extra}
    };
    std::string payload = j.dump();
    
    // 2. Create buffer: 4 bytes (length) + payload
    uint32_t length = static_cast<uint32_t>(payload.size());
    std::vector<uint8_t> result(4 + length);
    
    // 3. Write length in big-endian (network byte order)
    result[0] = (length >> 24) & 0xFF;  // Most significant byte
    result[1] = (length >> 16) & 0xFF;
    result[2] = (length >> 8) & 0xFF;
    result[3] = length & 0xFF;          // Least significant byte
    
    // 4. Copy payload after length header
    std::memcpy(result.data() + 4, payload.data(), length);
    
    return result;
}
```

**Example:**
```
Message: {"type":10,"sender":"alice","content":"Hello"}
Length: 47 bytes
Result: [0x00 0x00 0x00 0x2F] + [{"type":10,"sender":"alice","content":"Hello"}]
        └─────────┬──────────┘   └────────────────────┬─────────────────────────┘
           47 decimal                   47 bytes JSON
         (0x0000002F)
```

---

#### Deserialization & Buffering (MessageBuffer)

**The Challenge:**
```
recv() call 1: [0x00 0x00 0x00 0x32 "{'type':1,'use"]      ← Incomplete!
recv() call 2: ["r':'alice'...]                            ← Missing header!
recv() call 3: ["...,'content':'hi'}"] [0x00 0x00 0x00 0x15 "{'type':100..."] ← Two messages!
```

**The Solution: MessageBuffer Class**

```cpp
class MessageBuffer {
private:
    std::vector<uint8_t> buffer_;  // Accumulates incomplete data
    
public:
    // Add newly received data to buffer
    void append(const uint8_t* data, size_t length) {
        buffer_.insert(buffer_.end(), data, data + length);
    }
    
    // Check if we have at least one complete message
    bool hasCompleteMessage() const {
        // Need at least 4 bytes for length header
        if (buffer_.size() < 4) return false;
        
        // Extract length from header (big-endian)
        uint32_t messageLength = 
            (static_cast<uint32_t>(buffer_[0]) << 24) |
            (static_cast<uint32_t>(buffer_[1]) << 16) |
            (static_cast<uint32_t>(buffer_[2]) << 8)  |
            (static_cast<uint32_t>(buffer_[3]));
        
        // Check if we have: header (4 bytes) + full payload
        return buffer_.size() >= (4 + messageLength);
    }
    
    // Extract and remove one complete message from buffer
    Message extractMessage() {
        if (!hasCompleteMessage()) {
            return Message();  // Empty message
        }
        
        // Read length header
        uint32_t length = 
            (static_cast<uint32_t>(buffer_[0]) << 24) |
            (static_cast<uint32_t>(buffer_[1]) << 16) |
            (static_cast<uint32_t>(buffer_[2]) << 8)  |
            (static_cast<uint32_t>(buffer_[3]));
        
        // Deserialize payload (skip 4-byte header)
        Message msg = Protocol::deserialize(&buffer_[4], length);
        
        // Remove processed bytes from buffer
        buffer_.erase(buffer_.begin(), buffer_.begin() + 4 + length);
        
        return msg;
    }
    
    void clear() {
        buffer_.clear();
    }
};
```

**Usage Flow:**

```cpp
// In client/server receive loop:
void onDataReceived(const uint8_t* data, size_t len) {
    // 1. Append to buffer
    buffer_.append(data, len);
    
    // 2. Extract all complete messages
    while (buffer_.hasCompleteMessage()) {
        Protocol::Message msg = buffer_.extractMessage();
        handleMessage(msg);  // Process the message
    }
    
    // 3. Incomplete data remains in buffer for next recv()
}
```

**Example Scenario:**

```
Initial buffer: []

recv() #1: [0x00 0x00 0x00 0x32 "{'type':1,'u"]  (20 bytes)
Buffer: [0x00 0x00 0x00 0x32 "{'type':1,'u"]
hasCompleteMessage()? No (need 4 + 0x32 = 54 bytes, have 20)

recv() #2: ["ser':'alice','password':'123']  (34 bytes)
Buffer: [0x00 0x00 0x00 0x32 "{'type':1,'user':'alice','password':'123']  (54 bytes)
hasCompleteMessage()? Yes! (have exactly 54 bytes)
extractMessage() → Returns LOGIN message
Buffer: []  (cleared)

recv() #3: [0x00 0x00 0x00 0x15 "{'type':100,'...}"] [0x00 0x00 0x00 0x20 "{'t"]  (33 bytes)
Buffer: [0x00 0x00 0x00 0x15 "{'type':100,'...}"] [0x00 0x00 0x00 0x20 "{'t"]
hasCompleteMessage()? Yes! (first message is complete: 4 + 0x15 = 25 bytes)
extractMessage() → Returns OK message
Buffer: [0x00 0x00 0x00 0x20 "{'t"]  (5 bytes remain)
hasCompleteMessage()? No (need 4 + 0x20 = 36 bytes, have 5)
Wait for more data...
```

---

### 7. Send/Receive Implementation

#### Server-Side (Raw Sockets)

**Sending Messages:**

```cpp
bool ClientSession::sendMessage(const Protocol::Message& msg) {
    std::lock_guard<std::mutex> lock(sendMutex_);  // Thread-safe
    
    if (!active_ || socketFd_ < 0) {
        return false;
    }
    
    // Serialize to length-prefixed format
    std::vector<uint8_t> data = Protocol::serialize(msg);
    
    // send() might not send all data in one call!
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        ssize_t sent = send(socketFd_, 
                           reinterpret_cast<const char*>(data.data() + totalSent),
                           data.size() - totalSent, 
                           0);
        
        if (sent <= 0) {
            return false;  // Error or connection closed
        }
        
        totalSent += sent;
    }
    
    return true;
}
```

**Why the loop?**
- `send()` may send partial data (especially for large messages)
- Returns number of bytes actually sent
- Must loop until all data is sent

**Receiving Messages:**

```cpp
void Server::handleClient(int socketFd, const std::string& address) {
    uint8_t buffer[4096];
    
    while (running_) {
        // Blocking read - waits for data
        ssize_t bytesRead = recv(socketFd, 
                                reinterpret_cast<char*>(buffer), 
                                4096, 
                                0);
        
        if (bytesRead <= 0) {
            // 0 = graceful disconnect
            // -1 = error
            break;
        }
        
        // Process data through MessageBuffer
        session->processData(buffer, bytesRead);
    }
}
```

---

#### Client-Side (Qt)

**Sending Messages:**

```cpp
void ChatClient::sendMessage(const Protocol::Message& msg) {
    if (!isConnected()) return;
    
    // Serialize to bytes
    std::vector<uint8_t> data = Protocol::serialize(msg);
    
    // Qt handles buffering and partial sends internally
    socket_->write(reinterpret_cast<const char*>(data.data()), data.size());
    socket_->flush();  // Force send
}
```

**Receiving Messages (Event-Driven):**

```cpp
void ChatClient::onReadyRead() {
    // Qt has already buffered the data
    QByteArray data = socket_->readAll();  // Get all available data
    
    // Add to our protocol buffer
    buffer_.append(reinterpret_cast<const uint8_t*>(data.constData()), 
                   data.size());
    
    // Extract all complete messages
    while (buffer_.hasCompleteMessage()) {
        Protocol::Message msg = buffer_.extractMessage();
        processMessage(msg);
    }
}
```

**Key Difference:**
- Server: Manual `send()` loop, blocking `recv()`
- Client: Qt handles buffering, event-driven callbacks

---

### 8. Platform-Specific Differences

#### Windows (WinSock2)

```cpp
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// INITIALIZATION (REQUIRED)
WSADATA wsaData;
if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    // Error: Winsock not available
}

// SOCKET TYPE
SOCKET sock;  // Actually just an unsigned int

// CLOSING SOCKET
closesocket(sock);  // Not close()!

// SHUTDOWN
shutdown(sock, SD_BOTH);  // SD_SEND, SD_RECEIVE, SD_BOTH

// CLEANUP (REQUIRED)
WSACleanup();

// ERROR HANDLING
int error = WSAGetLastError();
```

#### Linux/macOS (POSIX)

```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// INITIALIZATION
// None required!

// SOCKET TYPE
int sock;  // File descriptor

// CLOSING SOCKET
close(sock);  // Standard POSIX close()

// SHUTDOWN
shutdown(sock, SHUT_RDWR);  // SHUT_RD, SHUT_WR, SHUT_RDWR

// CLEANUP
// None required!

// ERROR HANDLING
int error = errno;
```

---

#### Common Functions (Both Platforms)

These functions work identically on Windows and POSIX:

```cpp
// Create socket
int sock = socket(AF_INET, SOCK_STREAM, 0);

// Bind to address
bind(sock, (struct sockaddr*)&addr, sizeof(addr));

// Listen for connections
listen(sock, backlog);

// Accept connection
int client = accept(sock, (struct sockaddr*)&clientAddr, &len);

// Send data
ssize_t sent = send(sock, buffer, length, 0);

// Receive data
ssize_t received = recv(sock, buffer, length, 0);

// Set socket options
setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

// Get socket options
getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);
```

---

#### Cross-Platform Abstraction Pattern

```cpp
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CLOSE_SOCKET close
#endif

// Usage:
CLOSE_SOCKET(sock);  // Works on both platforms
```

---

### 9. Connection States & Flow

#### Server State Machine

```
┌──────────────────┐
│   CREATED        │  Constructor called
│   (port set)     │
└────────┬─────────┘
         │
         │ start()
         ↓
┌──────────────────┐
│   INITIALIZING   │  WSAStartup() (Windows only)
└────────┬─────────┘
         │
         │ socket()
         ↓
┌──────────────────┐
│   SOCKET_CREATED │  Socket FD obtained
└────────┬─────────┘
         │
         │ setsockopt(SO_REUSEADDR)
         │ bind()
         ↓
┌──────────────────┐
│   BOUND          │  Bound to 0.0.0.0:PORT
└────────┬─────────┘
         │
         │ listen()
         ↓
┌──────────────────┐
│   LISTENING      │◄─┐ accept() blocks here
│   (running_=true)│  │ Waiting for clients
└────────┬─────────┘  │
         │            │
         │ [Client connects]
         ↓            │
┌──────────────────┐  │
│ ACCEPT NEW CLIENT│  │
│ - Get client FD  │  │
│ - Create Session │  │
│ - Spawn thread   │  │
└────────┬─────────┘  │
         │            │
         └────────────┘
         
         
Per-Client Thread:
┌──────────────────┐
│  CONNECTED       │  recv() loop
│  (per client)    │
└────────┬─────────┘
         │
         │ [messages flowing]
         │ recv() → processData() → handleMessage()
         │
         │ [disconnect/error]
         ↓
┌──────────────────┐
│  DISCONNECTED    │  Thread exits
│  - Close socket  │  Session destroyed
│  - Cleanup       │
└──────────────────┘
```

---

#### Client State Machine (Qt)

```
┌──────────────────┐
│  Disconnected    │  Initial state
│  (state_=0)      │
└────────┬─────────┘
         │
         │ connectToServer(host, port)
         │ socket_->connectToHost()
         ↓
┌──────────────────┐
│  Connecting      │  TCP handshake in progress
│  (state_=1)      │  (SYN, SYN-ACK, ACK)
└────────┬─────────┘
         │
         │ [signal: connected()]
         ↓
┌──────────────────┐
│  Connected       │  TCP established
│  (state_=2)      │  pingTimer_->start()
└────────┬─────────┘
         │
         │ login(username, password)
         │ [Server responds: OK]
         ↓
┌──────────────────┐
│  Authenticated   │  Can send/receive messages
│  (state_=3)      │  Full functionality available
│  username_ set   │
│  isAdmin_ set    │
└────────┬─────────┘
         │
         │ [User actions]
         │ • sendGlobalMessage()
         │ • sendPrivateMessage()
         │ • Admin commands (if admin)
         │
         │ logout() or disconnect
         ↓
┌──────────────────┐
│  Disconnected    │  [signal: disconnected()]
│  (state_=0)      │  pingTimer_->stop()
│  Clear state     │  Clear username, admin flag
└──────────────────┘
```

---

#### Authentication Flow (Login Sequence)

**Client → Server:**

```
1. Client: setState(Connecting)
   socket_->connectToHost("127.0.0.1", 8080)

2. Qt emits: connected() signal
   Client: setState(Connected)
   
3. User clicks Login button
   Client: login("alice", "password123")
   
4. Client creates LOGIN message:
   {
     "type": 2,  // LOGIN
     "content": '{"username":"alice","password":"password123"}'
   }
   
5. Client: sendMessage(loginMsg)
   → serialize() → [0x00 0x00 0x00 0x4A]{...JSON...}
   → socket_->write()

6. Server: recv() receives data
   → buffer_.append()
   → extractMessage()
   → handleLogin()
   
7. Server validates:
   - Database::authenticateUser("alice", "password123")
   - Check if already online
   - Check if banned
   
8. Server sends OK response:
   {
     "type": 100,  // OK
     "content": "Login successful",
     "extra": '{"displayName":"Alice","isAdmin":false,"isMuted":false}'
   }

9. Client: onReadyRead()
   → buffer_.extractMessage()
   → processMessage()
   → handleOkResponse()
   
10. Client: setState(Authenticated)
    username_ = "alice"
    displayName_ = "Alice"
    isAdmin_ = false
    emit loginSuccess("alice", "Alice")

11. UI: MainWindow shows
    - Enable chat interface
    - Load online users list
    - Show welcome message
```

---

### 10. Broadcasting & Routing

#### Server-Side Message Routing

```cpp
// BROADCAST to all authenticated clients
void Server::broadcast(const Protocol::Message& msg, int excludeSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    for (auto& [fd, session] : clients_) {
        if (fd == excludeSocket) continue;  // Skip sender
        
        if (session->isAuthenticated()) {
            session->sendMessage(msg);
        }
    }
}

// SEND to specific user
bool Server::sendToUser(const std::string& username, 
                       const Protocol::Message& msg) {
    std::lock_guard<std::mutex> lock(usersMutex_);
    
    auto it = onlineUsers_.find(username);
    if (it != onlineUsers_.end()) {
        return it->second->sendMessage(msg);
    }
    return false;
}
```

**Global Message Flow:**

```
User A: sendGlobalMessage("Hello everyone!")
   ↓
Server: handleGlobalMessage()
   ↓
Server: broadcast(msg, socketA)  // Exclude sender
   ↓
   ├→ User B: receives message
   ├→ User C: receives message
   └→ User D: receives message
```

**Private Message Flow:**

```
User A → Server: MSG_PRIVATE to "Bob"
   ↓
Server: handlePrivateMessage()
   ↓
Server: sendToUser("Bob", msg)
   ↓
User B (Bob): receives message
```

---

## Key Features

### User Management
✅ Registration with username/password  
✅ Login authentication with SQLite backend  
✅ Password change functionality  
✅ Display names  
✅ Persistent user accounts  

### Chat Functionality
✅ Global chat room (broadcast to all)  
✅ Private 1-on-1 messaging  
✅ Real-time message delivery  
✅ Timestamp tracking  
✅ Message history (in UI)  

### Admin/Moderation System
✅ Two-tier role system (Member/Admin)  
✅ **Kick:** Disconnect user from server  
✅ **Ban:** Prevent user from logging in  
✅ **Mute:** Prevent user from sending messages  
✅ **Promote/Demote:** Change user roles  
✅ View all users, banned list, muted list  
✅ Admin panel in client UI  

### Real-time Features
✅ Online/offline user notifications  
✅ Live user list updates  
✅ Heartbeat mechanism (PING/PONG every 30s)  
✅ Auto-reconnect on client side  
✅ Connection state indicators  

### Multi-threading
✅ Thread-per-client model  
✅ Mutex-protected shared data  
✅ Thread-safe database access  
✅ Non-blocking UI (Qt event loop)  

---

## Security Features

### Authentication
- **Password Hashing:** Passwords stored hashed in database (not plaintext)
- **Session Management:** Server tracks authenticated sessions
- **Login Validation:** Username/password verification before access

### Network Security
- **Length-Prefixed Protocol:** Prevents buffer overflow attacks
- **Input Validation:** Username/password length checks
- **Message Validation:** JSON parsing with exception handling
- **Connection Limits:** Max 100 concurrent clients

### Access Control
- **Role-Based:** Admin-only commands enforced server-side
- **Ban System:** Banned users cannot login
- **Mute System:** Muted users cannot send messages
- **Kick System:** Forceful disconnection capability

### Error Handling
- **Exception Handling:** Try-catch blocks around critical operations
- **Graceful Degradation:** Single message errors don't crash connections
- **Logging:** Server logs all important events and errors

---

## Communication Flow Summary

### TCP 3-Way Handshake (Connection Establishment)

```
Client                          Server
  │                               │
  │─────── SYN ─────────────────→ │  (Client: "I want to connect")
  │                               │  listen() waiting
  │                               │  accept() called
  │←────── SYN-ACK ──────────────│  (Server: "OK, let's connect")
  │                               │
  │─────── ACK ─────────────────→ │  (Client: "Connection confirmed")
  │                               │
  │        CONNECTED              │  accept() returns new socket
  │                               │  spawn handleClient thread
```

### Application-Level Communication

```
┌──────────┐                    ┌──────────┐
│  Client  │                    │  Server  │
└────┬─────┘                    └────┬─────┘
     │                               │
     │ LOGIN message                 │
     │────────────────────────────→  │
     │ [0x00 0x00 0x00 0x32]{JSON}   │
     │                               │
     │                          recv()│
     │                   processData()│
     │                    handleLogin()│
     │                 authenticateUser()│
     │                               │
     │ OK response                   │
     │ ←────────────────────────────│
     │ [0x00 0x00 0x00 0x28]{JSON}   │
     │                               │
readyRead()                          │
processMessage()                     │
handleOkResponse()                   │
setState(Authenticated)              │
     │                               │
     │ MSG_GLOBAL                    │
     │────────────────────────────→  │
     │                               │
     │                   handleGlobalMessage()│
     │                        broadcast()│
     │                               │
     │←─ Broadcast to all clients ──│
     │                               │
```

### Disconnection (TCP 4-Way Handshake)

```
Client                          Server
  │                               │
  │─────── FIN ─────────────────→ │  (Client closes)
  │                               │  recv() returns 0
  │                               │  Exit handleClient loop
  │←────── ACK ──────────────────│
  │                               │
  │←────── FIN ──────────────────│  (Server closes)
  │                               │
  │─────── ACK ─────────────────→ │
  │                               │
  │      DISCONNECTED             │  Session destroyed
```

---

## Build System

### CMake Configuration

```cmake
cmake_minimum_required(VERSION 3.14)
project(TCPChat VERSION 1.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(BUILD_SERVER "Build the chat server" ON)
option(BUILD_CLIENT "Build the chat client (requires Qt)" ON)

# Platform-specific libraries
if(WIN32)
    set(PLATFORM_LIBS ws2_32)  # WinSock2
else()
    set(PLATFORM_LIBS pthread)  # POSIX threads
endif()

# Common library (Protocol)
add_library(common STATIC
    common/Protocol.cpp
    common/Protocol.h
)

# Server executable
add_executable(chat_server
    server/server_main.cpp
    server/Server.cpp
    server/ClientSession.cpp
    server/Database.cpp
)
target_link_libraries(chat_server
    common
    ${PLATFORM_LIBS}
    ${SQLite3_LIBRARIES}
)

# Client executable (requires Qt5 or Qt6)
add_executable(chat_client
    client/client_main.cpp
    client/ChatClient.cpp
    client/MainWindow.cpp
    client/AuthDialog.cpp
)
target_link_libraries(chat_client
    common
    ${PLATFORM_LIBS}
    Qt::Widgets
    Qt::Network
)
```

### Dependencies

**Server:**
- C++17 compiler
- CMake 3.14+
- SQLite3 library
- Platform socket library (WinSock2 or POSIX)

**Client:**
- All server dependencies
- Qt5 or Qt6 (Widgets + Network modules)

---

## Performance Characteristics

### Scalability
- **Max Clients:** 100 (configurable via `maxClients_` parameter)
- **Threading Model:** One thread per client (limits scalability)
- **Better Alternative:** Thread pool or async I/O (epoll/IOCP)

### Message Throughput
- **Limited by:** TCP bandwidth and serialization overhead
- **JSON Overhead:** Human-readable but larger than binary
- **Optimization:** Could use binary protocol (Protocol Buffers, MessagePack)

### Memory Usage
- **Per Client:** ~4KB buffer + thread stack (1-2MB on Linux)
- **MessageBuffer:** Grows dynamically with incomplete messages
- **Database:** SQLite in-process (low memory footprint)

---

## Potential Improvements

### Architecture
1. **Thread Pool:** Replace thread-per-client with worker pool
2. **Async I/O:** Use epoll (Linux) or IOCP (Windows)
3. **Load Balancing:** Multiple server instances with shared database

### Protocol
1. **Binary Protocol:** Replace JSON with Protocol Buffers
2. **Compression:** Add zlib compression for large messages
3. **Encryption:** Add TLS/SSL support (`QSslSocket` or OpenSSL)

### Features
1. **Message History:** Store messages in database
2. **File Transfer:** Add binary file support
3. **Voice/Video:** WebRTC integration
4. **Presence:** Rich status (away, busy, etc.)
5. **Rooms:** Multiple chat rooms/channels

### Security
1. **TLS Encryption:** Encrypt all network traffic
2. **Rate Limiting:** Prevent spam/flood attacks
3. **Authentication Tokens:** JWT instead of repeated login
4. **Password Policy:** Enforce strong passwords
5. **SQL Injection:** Use prepared statements (already done)

---

## Conclusion

This is a **well-architected, production-ready TCP socket chat application** demonstrating:

✅ **Proper socket programming** (bind, listen, accept, send, recv)  
✅ **Cross-platform compatibility** (Windows WinSock + POSIX)  
✅ **Protocol design** (length-prefixed messaging)  
✅ **Stream reassembly** (MessageBuffer handles fragmentation)  
✅ **Multi-threading** (thread-per-client with mutexes)  
✅ **Qt integration** (event-driven client with signals/slots)  
✅ **Database persistence** (SQLite for user accounts)  
✅ **Role-based access control** (admin/member system)  
✅ **Real-time features** (online notifications, heartbeat)  

The implementation showcases fundamental networking concepts while providing a functional, feature-rich chat application suitable for educational purposes and real-world deployment (with additional security hardening).

---

## Code Review & Critical Issues Analysis

*Review Date: December 30, 2025*  
*Target Use Case: Internet Cafe (Quán Net) LAN Chat System*

### **🔴 CRITICAL SECURITY ISSUES**

#### 1. **Weak Password Hashing (HIGH PRIORITY)**

**Current Implementation:**
```cpp
// Database.cpp - Line 114
std::string Database::hashPassword(const std::string& password) {
    // ⚠️ CRITICAL: Uses std::hash - NOT cryptographically secure!
    std::hash<std::string> hasher;
    size_t hash = hasher(password + "chat_salt_2024");
    return oss.str();
}
```

**Problems:**
- ❌ `std::hash` is designed for hash tables, NOT password security
- ❌ Single fixed salt ("chat_salt_2024") for all passwords
- ❌ Vulnerable to rainbow table attacks
- ❌ Extremely fast to brute-force (millions of hashes/second)
- ❌ Output only 64-bit (16 hex chars) - too short

**Attack Scenario:**
```
Attacker gets database → Extract hash → Crack in seconds
Example: Password "123456" → Same hash for all users with "123456"
```

**SOLUTION REQUIRED:**
```cpp
// Use bcrypt or Argon2 with per-user salt
#include <bcrypt/BCrypt.hpp>

std::string Database::hashPassword(const std::string& password) {
    // Bcrypt automatically generates random salt per password
    return BCrypt::generateHash(password, 12);  // Work factor 12
}

bool Database::verifyPassword(const std::string& password, 
                              const std::string& hash) {
    return BCrypt::validatePassword(password, hash);
}
```

**Libraries to use:**
- **bcrypt** (Recommended for internet cafe): https://github.com/hilch/Bcrypt.cpp
- **Argon2** (More modern): https://github.com/P-H-C/phc-winner-argon2

---

#### 2. **No Network Encryption (MEDIUM-HIGH)**

**Current State:**
- ❌ All data sent in **plaintext** over TCP
- ❌ Passwords transmitted unencrypted
- ❌ Messages can be intercepted with Wireshark

**Risk Level:**
- **Low** for isolated LAN (single internet cafe)
- **HIGH** if network has untrusted users

**SOLUTION for Internet Cafe:**
```cpp
// Option 1: Use Qt's QSslSocket (easiest)
#include <QSslSocket>

class ChatClient {
    QSslSocket* socket_;  // Instead of QTcpSocket
    
    void connectToServer(const QString& host, quint16 port) {
        socket_->connectToHostEncrypted(host, port);
    }
};

// Option 2: OpenSSL wrapper for raw sockets (server)
// Requires more work but gives full control
```

**Practical Decision:**
- For **single cafe LAN**: Can skip SSL (low priority)
- For **multiple cafes** or **public network**: MUST implement SSL/TLS

---

#### 3. **SQL Injection Protection (GOOD - But Verify)**

**Current Implementation:**
```cpp
// Database.cpp uses prepared statements ✅
sqlite3_prepare_v2(db_, "SELECT * FROM users WHERE username = ?", ...);
sqlite3_bind_text(stmt, 1, username.c_str(), ...);
```

**Status:** ✅ **SECURE** - Using parameterized queries

**Verification Needed:**
- Check all database queries use `sqlite3_prepare_v2` + `sqlite3_bind_*`
- Never concatenate user input into SQL strings

---

### **⚠️ ARCHITECTURE ISSUES**

#### 4. **Thread Scalability Problem (MEDIUM)**

**Current Model:**
```
1 Client = 1 Thread
100 Clients = 100 Threads + 1 Accept Thread = 101 threads
```

**Problems:**
- ❌ Each thread consumes 1-2 MB stack memory
- ❌ Context switching overhead with many threads
- ❌ Hard limit at ~100 clients (configured `maxClients_`)

**Math:**
```
100 clients × 2 MB/thread = 200 MB just for stacks
+ Context switches = Performance degradation
```

**For Internet Cafe:**
- Typical cafe: 20-50 computers
- ✅ Current model is **ACCEPTABLE**
- 🔵 **No urgent fix needed** for this use case

**Better Architecture (if scaling beyond 100):**
```cpp
// Thread pool pattern
class Server {
    ThreadPool pool_;  // Fixed size (e.g., 10 threads)
    std::queue<Task> taskQueue_;
    
    // Workers pull tasks from queue
    void workerThread() {
        while (running_) {
            Task task = taskQueue_.pop();
            task.execute();
        }
    }
};
```

**Or use async I/O:**
- Linux: `epoll` (event-driven)
- Windows: IOCP (I/O Completion Ports)
- Cross-platform: Boost.Asio

---

#### 5. **Memory Leak Risk in Thread Management (LOW)**

**Potential Issue:**
```cpp
// Server.cpp
void Server::acceptLoop() {
    while (running_) {
        // Spawn new thread
        clientThreads_.emplace_back(&Server::handleClient, ...);
        // ⚠️ Old threads never removed from vector
    }
}
```

**Problem:**
- Threads finish when clients disconnect
- But `std::thread` objects remain in `clientThreads_` vector
- Memory grows over time (not reclaimed until server stops)

**Impact:**
- For 24/7 server with high turnover: Memory leak
- For internet cafe (restart daily): Low impact

**FIX:**
```cpp
void Server::acceptLoop() {
    while (running_) {
        // Periodically clean up finished threads
        {
            std::lock_guard<std::mutex> lock(threadsMutex_);
            clientThreads_.erase(
                std::remove_if(clientThreads_.begin(), 
                              clientThreads_.end(),
                              [](std::thread& t) { 
                                  if (t.joinable() && /* check if finished */) {
                                      t.join();
                                      return true;
                                  }
                                  return false;
                              }),
                clientThreads_.end()
            );
        }
        
        int clientSocket = accept(...);
        // ...
    }
}
```

---

#### 6. **No Message Rate Limiting (MEDIUM)**

**Current State:**
- ❌ Clients can send unlimited messages
- ❌ No flood protection

**Attack Scenario:**
```cpp
// Malicious client
while (true) {
    sendGlobalMessage("SPAM SPAM SPAM");  // 1000s msgs/sec
}
```

**Impact:**
- Server CPU overload
- Database overload (if logging messages)
- All clients lag due to broadcast

**SOLUTION:**
```cpp
class ClientSession {
    std::chrono::time_point<std::chrono::steady_clock> lastMessageTime_;
    int messageCount_;
    
    bool checkRateLimit() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - lastMessageTime_).count();
        
        if (elapsed < 1) {
            messageCount_++;
            if (messageCount_ > 10) {  // Max 10 msgs/second
                return false;  // Rate limit exceeded
            }
        } else {
            messageCount_ = 1;
            lastMessageTime_ = now;
        }
        return true;
    }
};
```

---

### **🟡 MISSING FEATURES FOR INTERNET CAFE**

#### 7. **No Server GUI (HIGH PRIORITY for Cafe)**

**Current:** Console-only server
**Need:** GUI to manage cafe

**Required Features:**
```
┌─────────────────────────────────────────┐
│  Chat Server - Internet Cafe Manager    │
├─────────────────────────────────────────┤
│  Connected Clients:                     │
│  ┌─────────────────────────────────┐   │
│  │ PC-01 | alice   | 192.168.1.101 │   │
│  │ PC-05 | bob     | 192.168.1.105 │   │
│  │ PC-12 | charlie | 192.168.1.112 │   │
│  └─────────────────────────────────┘   │
│  [Kick] [Ban] [Mute] [Send Message]    │
├─────────────────────────────────────────┤
│  Server Log:                            │
│  [14:30:15] User alice logged in        │
│  [14:31:22] Global message from bob     │
│  └─────────────────────────────────┘   │
├─────────────────────────────────────────┤
│  Statistics:                            │
│  • Online: 23/50                        │
│  • Messages today: 1,234                │
│  • Bandwidth: 15 KB/s                   │
└─────────────────────────────────────────┘
```

**Implementation Plan:**
```cpp
// server_gui/ServerWindow.h
class ServerWindow : public QMainWindow {
    Q_OBJECT
    
public:
    ServerWindow();
    
private slots:
    void onClientConnected(QString username, QString ip);
    void onClientDisconnected(QString username);
    void onMessageReceived(QString from, QString to, QString msg);
    void onKickClicked();
    void onBanClicked();
    void onBroadcastClicked();
    
private:
    Server* server_;
    QTableWidget* clientTable_;
    QTextEdit* logView_;
    QLabel* statsLabel_;
};
```

---

#### 8. **No Message History/Logging (MEDIUM)**

**Current State:**
- ❌ Messages disappear after sending
- ❌ No chat history
- ❌ Can't review past conversations

**For Internet Cafe:**
- Useful for monitoring/compliance
- Help resolve disputes between users

**SOLUTION:**
```sql
-- Add messages table
CREATE TABLE messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sender TEXT NOT NULL,
    receiver TEXT,  -- NULL for global messages
    content TEXT NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    message_type TEXT  -- 'global' or 'private'
);

CREATE INDEX idx_sender ON messages(sender);
CREATE INDEX idx_timestamp ON messages(timestamp);
```

```cpp
// Database.cpp
bool Database::logMessage(const std::string& sender,
                          const std::string& receiver,
                          const std::string& content,
                          const std::string& type) {
    const char* sql = "INSERT INTO messages (sender, receiver, content, message_type) "
                     "VALUES (?, ?, ?, ?)";
    // ... implementation
}

std::vector<Message> Database::getRecentMessages(int limit = 50) {
    // Retrieve last N messages for history
}
```

---

#### 9. **No File Transfer Support (LOW)**

**Current:** Text-only messages
**Enhancement:** Share files/images

**Use Case:**
- Users share game config files
- Share screenshots
- Send documents

**Protocol Extension:**
```cpp
enum class MessageType {
    // ... existing types
    FILE_TRANSFER_REQUEST = 50,
    FILE_TRANSFER_ACCEPT = 51,
    FILE_TRANSFER_CHUNK = 52,
    FILE_TRANSFER_COMPLETE = 53
};
```

**Priority:** Low (can use external tools)

---

#### 10. **No Computer/Seat Assignment (HIGH for Cafe)**

**Cafe Scenario:**
```
User walks in → Gets PC #5 → Should auto-login as "PC-05-User"
OR
User should see which PC they're on in the chat
```

**SOLUTION:**
```cpp
// Client startup - auto-detect PC name
QString pcName = QHostInfo::localHostName();  // "PC-05"

// Include in messages
struct Message {
    std::string sender;        // "alice"
    std::string pcName;        // "PC-05"  ← NEW
    std::string content;
};

// Display as: [PC-05] alice: Hello!
```

**Alternative:** Admin assigns PC names in server config
```json
{
    "pc_mappings": {
        "192.168.1.101": "PC-01",
        "192.168.1.105": "PC-05",
        "192.168.1.112": "PC-12"
    }
}
```

---

### **🔧 CODE QUALITY IMPROVEMENTS**

#### 11. **Inconsistent Error Handling**

**Issues:**
```cpp
// Some places return bool
bool Database::registerUser(...) { return false; }

// Some throw exceptions
session->processData(...);  // May throw

// Some silently fail
sendMessage(...);  // Returns void, ignores errors
```

**Recommendation:**
- Standardize error handling strategy
- Use exceptions for exceptional cases
- Use return codes for expected failures

---

#### 12. **No Configuration File**

**Current:** Hard-coded values
```cpp
Server server(9000, 100);  // Port and max clients hard-coded
```

**Better:**
```json
// config.json
{
    "server": {
        "port": 9000,
        "max_clients": 50,
        "log_file": "server.log",
        "database": "chat.db"
    },
    "security": {
        "enable_ssl": false,
        "rate_limit": 10,
        "password_min_length": 4
    }
}
```

---

#### 13. **No Logging to File**

**Current:** `std::cout` only (console)
**Better:** File logging with rotation

```cpp
class Logger {
    std::ofstream logFile_;
    
public:
    void log(const std::string& msg) {
        logFile_ << getCurrentTimestamp() << " " << msg << std::endl;
        std::cout << msg << std::endl;  // Also print to console
    }
};
```

---

### **📊 Performance Analysis**

#### Current Capacity Estimate

| Metric | Value | Notes |
|--------|-------|-------|
| **Max Clients** | 100 | Hard limit |
| **Realistic Load** | 50-60 | With current threading |
| **Memory/Client** | ~2 MB | Thread stack |
| **CPU/Client** | ~0.1% idle | During idle (recv blocked) |
| **Bandwidth/Client** | 1-10 KB/s | Text messages |

**For 50-PC Internet Cafe:**
✅ **SUFFICIENT** - No performance concerns

---

### **🎯 RECOMMENDATIONS BY PRIORITY**

### **MUST FIX (Before Production)**

1. **🔴 Password Hashing** → Use bcrypt/Argon2
2. **🔴 Server GUI** → Build Qt admin panel
3. **🟡 Rate Limiting** → Prevent spam/flood

### **SHOULD IMPLEMENT (Internet Cafe Specific)**

4. **🟡 PC/Seat Assignment** → Track which PC each user is on
5. **🟡 Message Logging** → Database history for compliance
6. **🟡 Configuration File** → Make settings adjustable

### **NICE TO HAVE**

7. **🔵 SSL/TLS** → Only if multi-site or public network
8. **🔵 File Transfer** → Can use external tools
9. **🔵 Thread Pool** → Current model works for 50 clients

### **LOW PRIORITY**

10. **⚪ Logging to File** → Current console logging sufficient
11. **⚪ Better Error Handling** → Works but could be cleaner

---

## Implementation Roadmap for Internet Cafe

### **Phase 1: Critical Security (Week 1)**
```cpp
✅ Replace std::hash with bcrypt
✅ Add rate limiting (10 msgs/sec per user)
✅ Add input validation (username/password length)
```

### **Phase 2: Server GUI (Week 2-3)**
```cpp
✅ Create Qt server GUI application
✅ Display connected clients in table
✅ Add kick/ban/mute buttons
✅ Show real-time message log
✅ Display statistics (online count, bandwidth)
```

### **Phase 3: Cafe Features (Week 4)**
```cpp
✅ PC name assignment (auto-detect or config)
✅ Message history database
✅ Admin broadcast to all PCs
✅ Configuration file (JSON)
```

### **Phase 4: Polish (Week 5)**
```cpp
✅ File logging with rotation
✅ Better error messages
✅ User manual / documentation
✅ Deployment scripts
```

---

## Cafe Deployment Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  SERVER PC (1 machine)                   │
│                                                          │
│  ┌──────────────────────────────────────────┐          │
│  │  Chat Server GUI                          │          │
│  │  - Monitor all clients                    │          │
│  │  - Send announcements                     │          │
│  │  - Kick/ban troublemakers                 │          │
│  └──────────────────────────────────────────┘          │
│                                                          │
│  ┌──────────────────────────────────────────┐          │
│  │  Database (chat.db)                       │          │
│  │  - User accounts                          │          │
│  │  - Message history                        │          │
│  │  - Ban/mute records                       │          │
│  └──────────────────────────────────────────┘          │
│                                                          │
│  IP: 192.168.1.1 (Fixed)                                │
│  Port: 9000                                             │
└─────────────────────────────────────────────────────────┘
                         │
         ┌───────────────┼───────────────┐
         │               │               │
    ┌────▼────┐    ┌────▼────┐    ┌────▼────┐
    │ PC-01   │    │ PC-02   │    │ PC-50   │
    │ Client  │    │ Client  │    │ Client  │
    │ GUI     │    │ GUI     │    │ GUI     │
    │         │    │         │    │         │
    │ Auto-   │    │ Auto-   │    │ Auto-   │
    │ connect │    │ connect │    │ connect │
    │ to      │    │ to      │    │ to      │
    │ server  │    │ server  │    │ server  │
    └─────────┘    └─────────┘    └─────────┘
```

### **Client Auto-Config:**
```ini
# client_config.ini (on each PC)
[Server]
Host=192.168.1.1
Port=9000
AutoConnect=true
PCName=PC-01  # Different for each machine

[UI]
Theme=Dark
Language=Vietnamese
```

---

## Security Checklist for Cafe

- [ ] ✅ Strong password hashing (bcrypt)
- [ ] ✅ Rate limiting (prevent spam)
- [ ] ✅ SQL injection protection (prepared statements)
- [ ] ⚠️ Network encryption (SSL/TLS) - Optional for isolated LAN
- [ ] ✅ Admin authentication for server GUI
- [ ] ✅ Input validation (all user inputs)
- [ ] ✅ Ban/kick system working
- [ ] ✅ Message logging (compliance)
- [ ] ⚠️ Client-side validation (prevent tampering) - Medium priority
- [ ] ⚠️ Secure session management - Current is acceptable

---

## Final Assessment

### **Overall Code Quality:** ⭐⭐⭐⭐☆ (4/5)

**Strengths:**
- ✅ Clean architecture (client/server/common separation)
- ✅ Good socket implementation (cross-platform)
- ✅ Protocol well-designed (length-prefixed JSON)
- ✅ SQL injection protected
- ✅ Multi-threading working
- ✅ Qt integration smooth

**Critical Weaknesses:**
- ❌ Weak password hashing (MUST FIX)
- ❌ No server GUI (inconvenient for cafe)
- ❌ No rate limiting (vulnerable to spam)

**For Internet Cafe Use:**
- **Current State:** 60% ready
- **After Phase 1-2 fixes:** 90% ready
- **After Phase 3-4:** Production-ready

### **Verdict:**
This is a **solid educational project** with **good fundamentals**. With the critical security fixes (bcrypt) and server GUI implementation, it will be **fully suitable for internet cafe deployment**. The architecture is sound and can handle typical cafe load (20-50 concurrent users).

---

**End of Project Overview & Code Review**
