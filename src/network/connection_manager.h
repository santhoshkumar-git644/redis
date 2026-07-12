#ifndef INFERNOCACHE_CONNECTION_MANAGER_H
#define INFERNOCACHE_CONNECTION_MANAGER_H

#include "socket.h"
#include "../protocol/resp_parser.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

namespace inferno {
namespace network {

class Connection {
public:
    explicit Connection(Socket&& socket) : socket_(std::move(socket)) {}
    
    Socket& getSocket() { return socket_; }
    const Socket& getSocket() const { return socket_; }
    
    protocol::RESPParser& getParser() { return parser_; }

private:
    Socket socket_;
    protocol::RESPParser parser_;
};

class ConnectionManager {
public:
    ConnectionManager() = default;
    ~ConnectionManager();

    // Prevent copy and move
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    void addConnection(socket_t fd, std::shared_ptr<Connection> connection);
    void removeConnection(socket_t fd);
    std::shared_ptr<Connection> getConnection(socket_t fd);
    
    void closeAll();

private:
    std::unordered_map<socket_t, std::shared_ptr<Connection>> connections_;
    std::mutex mutex_;
};

} // namespace network
} // namespace inferno

#endif // INFERNOCACHE_CONNECTION_MANAGER_H
