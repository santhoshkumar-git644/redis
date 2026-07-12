#pragma once

#include "../network/socket.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>

namespace inferno {
namespace server {

class PubSubManager {
public:
    using SendCallback = std::function<void(network::socket_t, const std::string&)>;

    static PubSubManager& instance() {
        static PubSubManager instance;
        return instance;
    }

    void setSendCallback(SendCallback cb) { send_callback_ = cb; }

    // Subscribes a client to a channel. Returns the total number of channels the client is subscribed to.
    int subscribe(network::socket_t client_fd, const std::string& channel);

    // Unsubscribes a client from a channel. Returns the total number of channels the client is subscribed to.
    int unsubscribe(network::socket_t client_fd, const std::string& channel);

    // Unsubscribes a client from all channels (used when a client disconnects).
    void unsubscribeAll(network::socket_t client_fd);

    // Publishes a message to a channel. Returns the number of clients that received the message.
    int publish(const std::string& channel, const std::string& message);
    
    // Sends a raw string directly to a client
    void sendToClient(network::socket_t client_fd, const std::string& data);

private:
    PubSubManager() = default;
    ~PubSubManager() = default;
    
    PubSubManager(const PubSubManager&) = delete;
    PubSubManager& operator=(const PubSubManager&) = delete;

    std::mutex mutex_;
    
    // Maps channel name to a set of subscribed client FDs
    std::unordered_map<std::string, std::unordered_set<network::socket_t>> channels_;
    
    // Maps client FD to a set of channels they are subscribed to (for O(1) cleanup on disconnect)
    std::unordered_map<network::socket_t, std::unordered_set<std::string>> client_channels_;

    SendCallback send_callback_;
};

} // namespace server
} // namespace inferno
