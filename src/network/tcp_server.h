#ifndef INFERNOCACHE_TCP_SERVER_H
#define INFERNOCACHE_TCP_SERVER_H

#include "socket.h"
#include "event_loop.h"
#include "connection_manager.h"
#include <string>
#include <memory>
#include <thread>

namespace inferno {
namespace network {

class TCPServer {
public:
    TCPServer(const std::string& ip, int port);
    ~TCPServer();

    // Prevent copy and move
    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;

    bool start();
    void stop();

private:
    void handleNewConnection(socket_t fd, EventType type);
    void handleClientData(socket_t fd, EventType type);

    std::string ip_;
    int port_;
    Socket acceptor_socket_;
    
    std::unique_ptr<EventLoop> event_loop_;
    std::unique_ptr<ConnectionManager> connection_manager_;
};

} // namespace network
} // namespace inferno

#endif // INFERNOCACHE_TCP_SERVER_H
