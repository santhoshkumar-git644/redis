#include "tcp_server.h"
#include "../utils/logger.h"
#include "../server/command_handler.h"
#include "../persistence/aof.h"
#include <vector>
#include <functional>
#include <thread>
#include <chrono>
#include <atomic>

namespace inferno {
namespace network {

TCPServer::TCPServer(const std::string& ip, int port)
    : ip_(ip), port_(port),
      event_loop_(std::make_unique<EventLoop>()),
      connection_manager_(std::make_unique<ConnectionManager>()) {
    Socket::initializeNetwork();
}

TCPServer::~TCPServer() {
    stop();
    Socket::cleanupNetwork();
}

bool TCPServer::start() {
    if (!event_loop_->initialize()) {
        return false;
    }

    if (!acceptor_socket_.bind(ip_, port_)) {
        return false;
    }

    if (!acceptor_socket_.setReuseAddress(true)) {
        LOG_WARN("Failed to set SO_REUSEADDR");
    }

    if (!acceptor_socket_.setNonBlocking(true)) {
        LOG_ERROR("Failed to set acceptor socket to non-blocking");
        return false;
    }

    if (!acceptor_socket_.listen(128)) {
        return false;
    }

    LOG_INFO("Server listening on " + ip_ + ":" + std::to_string(port_));

    // Register acceptor socket with event loop
    auto accept_handler = [this](socket_t fd, EventType type) {
        this->handleNewConnection(fd, type);
    };

    // Load AOF before accepting any connections
    persistence::AOFManager::instance().load();
    // Start AOF persistence thread
    persistence::AOFManager::instance().start();

    if (!event_loop_->addEvent(acceptor_socket_.getFd(), EventType::READ, accept_handler)) {
        LOG_ERROR("Failed to add acceptor socket to event loop");
        return false;
    }

    std::thread([this]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            server::CommandHandler::instance().runActiveExpiration();
        }
    }).detach();

    // Run the event loop (this will block)
    event_loop_->run();

    return true;
}

void TCPServer::stop() {
    LOG_INFO("Stopping server...");
    event_loop_->stop();
    persistence::AOFManager::instance().stop();
    connection_manager_->closeAll();
    acceptor_socket_.close();
}

void TCPServer::handleNewConnection(socket_t /* fd */, EventType type) {
    if (type == EventType::ERROR) {
        LOG_ERROR("Error on acceptor socket");
        return;
    }

    Socket client_socket = acceptor_socket_.accept();
    if (!client_socket.isValid()) {
        // Normal in non-blocking mode if no connection is present
        return;
    }

    client_socket.setNonBlocking(true);
    client_socket.setTcpNoDelay(true);

    socket_t client_fd = client_socket.getFd();
    auto connection = std::make_shared<Connection>(std::move(client_socket));
    connection_manager_->addConnection(client_fd, connection);

    auto client_handler = [this](socket_t fd, EventType type) {
        this->handleClientData(fd, type);
    };

    if (!event_loop_->addEvent(client_fd, EventType::READ, client_handler)) {
        LOG_ERROR("Failed to add client socket to event loop");
        connection_manager_->removeConnection(client_fd);
    }
}

void TCPServer::handleClientData(socket_t fd, EventType type) {
    auto connection = connection_manager_->getConnection(fd);
    if (!connection) {
        return;
    }

    if (type == EventType::ERROR) {
        LOG_INFO("Client disconnected with error");
        event_loop_->removeEvent(fd);
        connection_manager_->removeConnection(fd);
        return;
    }

    if (type == EventType::READ) {
        std::vector<char> buffer(4096);
        int bytes_read = connection->getSocket().receive(buffer.data(), buffer.size());

        if (bytes_read > 0) {
            auto& parser = connection->getParser();
            parser.feed(buffer.data(), bytes_read);

            protocol::RESPObject parsed_obj;
            while (true) {
                auto res = parser.parse(parsed_obj);
                if (res == protocol::RESPParser::ParseResult::OK) {
                    // Dispatch the command to the CommandHandler
                    protocol::RESPObject response_obj = server::CommandHandler::instance().handleCommand(parsed_obj);
                    
                    std::string response = protocol::serializeRESP(response_obj);
                    connection->getSocket().send(response.c_str(), response.length());
                } else if (res == protocol::RESPParser::ParseResult::NEED_MORE) {
                    break;
                } else {
                    LOG_ERROR("Malformed RESP data received from client");
                    std::string err = "-ERR Protocol error\r\n";
                    connection->getSocket().send(err.c_str(), err.length());
                    event_loop_->removeEvent(fd);
                    connection_manager_->removeConnection(fd);
                    break;
                }
            }
        } else if (bytes_read == 0) {
            // Client closed connection gracefully
            LOG_INFO("Client disconnected gracefully");
            event_loop_->removeEvent(fd);
            connection_manager_->removeConnection(fd);
        } else {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
#else
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
#endif
                LOG_ERROR("Read error on client socket");
                event_loop_->removeEvent(fd);
                connection_manager_->removeConnection(fd);
            }
        }
    }
}

} // namespace network
} // namespace inferno
