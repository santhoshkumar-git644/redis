#include "pubsub.h"
#include "../utils/logger.h"

namespace inferno {
namespace server {

int PubSubManager::subscribe(network::socket_t client_fd, const std::string& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    channels_[channel].insert(client_fd);
    auto& subs = client_channels_[client_fd];
    subs.insert(channel);
    
    return subs.size();
}

int PubSubManager::unsubscribe(network::socket_t client_fd, const std::string& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = channels_.find(channel);
    if (it != channels_.end()) {
        it->second.erase(client_fd);
        if (it->second.empty()) {
            channels_.erase(it);
        }
    }
    
    auto client_it = client_channels_.find(client_fd);
    if (client_it != client_channels_.end()) {
        client_it->second.erase(channel);
        int remaining = client_it->second.size();
        if (remaining == 0) {
            client_channels_.erase(client_it);
        }
        return remaining;
    }
    
    return 0;
}

void PubSubManager::unsubscribeAll(network::socket_t client_fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto client_it = client_channels_.find(client_fd);
    if (client_it != client_channels_.end()) {
        for (const auto& channel : client_it->second) {
            auto it = channels_.find(channel);
            if (it != channels_.end()) {
                it->second.erase(client_fd);
                if (it->second.empty()) {
                    channels_.erase(it);
                }
            }
        }
        client_channels_.erase(client_it);
    }
}

int PubSubManager::publish(const std::string& channel, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = channels_.find(channel);
    if (it == channels_.end() || it->second.empty()) {
        return 0; // No subscribers
    }
    
    // Construct the RESP push message:
    // *3\r\n$7\r\nmessage\r\n$<channel_len>\r\n<channel>\r\n$<msg_len>\r\n<msg>\r\n
    std::string resp = "*3\r\n$7\r\nmessage\r\n$" + std::to_string(channel.length()) + "\r\n" + channel + "\r\n$" + std::to_string(message.length()) + "\r\n" + message + "\r\n";
    
    if (send_callback_) {
        for (network::socket_t fd : it->second) {
            send_callback_(fd, resp);
        }
    }
    
    return it->second.size();
}

void PubSubManager::sendToClient(network::socket_t client_fd, const std::string& data) {
    if (send_callback_) {
        send_callback_(client_fd, data);
    }
}

} // namespace server
} // namespace inferno
