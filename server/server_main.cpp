/**
 * @file server_main.cpp
 * @brief Main entry point for Chat Server
 *
 * Usage: ./chat_server [port]
 * Default port: 9000
 */

#include "Server.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

// Global server pointer for signal handler
Server* g_server = nullptr;

void signalHandler(int signum) {
    std::cout << "\nReceived signal " << signum << ", shutting down..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
    exit(signum);
}

void printBanner() {
    std::cout << R"(
╔═══════════════════════════════════════════════════╗
║           TCP CHAT SERVER v1.0                    ║
║                                                   ║
║  Commands:                                        ║
║    Ctrl+C - Shutdown server                       ║
╚═══════════════════════════════════════════════════╝
)" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default port
    int port = 9000;

    // Parse command line arguments
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port number. Using default port 9000." << std::endl;
            port = 9000;
        }
    }

    printBanner();

    // Create server
    Server server(port, 100);
    g_server = &server;

    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Start server
    if (!server.start()) {
        std::cerr << "Failed to start server on port " << port << std::endl;
        return 1;
    }

    std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;
    std::cout << "Waiting for connections..." << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    // Keep main thread alive
    while (server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
