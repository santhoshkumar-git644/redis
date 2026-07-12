#include "../network/tcp_server.h"
#include "../utils/logger.h"
#include <csignal>
#include <iostream>
#include <memory>
#include <atomic>

using namespace inferno::network;
using namespace inferno::utils;

std::atomic<bool> g_stop_requested{false};
std::unique_ptr<TCPServer> g_server = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        if (!g_stop_requested.exchange(true)) {
            LOG_INFO("\nReceived termination signal. Shutting down gracefully...");
            if (g_server) {
                g_server->stop();
            }
        }
    }
}

int main(int argc, char* argv[]) {
    // Basic argument parsing for port
    int port = 6379; // Default Redis port
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            LOG_ERROR("Invalid port specified. Using default: 6379");
        }
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    LOG_INFO("Starting InfernoCache Server...");

    try {
        g_server = std::make_unique<TCPServer>("0.0.0.0", port);
        
        if (!g_server->start()) {
            LOG_ERROR("Failed to start server");
            return 1;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception: ") + e.what());
        return 1;
    }

    LOG_INFO("Server shutdown complete.");
    return 0;
}
