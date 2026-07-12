#include "connection_manager.h"
#include "../utils/logger.h"

namespace inferno {
namespace network {

ConnectionManager::~ConnectionManager() {
    closeAll();
}

void ConnectionManager::addConnection(socket_t fd, std::shared_ptr<Connection> connection) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_[fd] = connection;
    LOG_INFO("Connection added. Total connections: " + std::to_string(connections_.size()));
}

void ConnectionManager::removeConnection(socket_t fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connections_.erase(fd) > 0) {
        LOG_INFO("Connection removed. Total connections: " + std::to_string(connections_.size()));
    }
}

std::shared_ptr<Connection> ConnectionManager::getConnection(socket_t fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr;
}

void ConnectionManager::closeAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : connections_) {
        pair.second->getSocket().close();
    }
    connections_.clear();
    LOG_INFO("All connections closed.");
}

} // namespace network
} // namespace inferno
