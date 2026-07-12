#ifndef INFERNOCACHE_EVENT_LOOP_H
#define INFERNOCACHE_EVENT_LOOP_H

#include "socket.h"
#include <functional>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

namespace inferno {
namespace network {

enum class EventType {
    READ,
    WRITE,
    ERROR
};

class EventLoop {
public:
    using EventHandler = std::function<void(socket_t, EventType)>;

    EventLoop();
    ~EventLoop();

    // Prevent copy and move
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    bool initialize();
    void run();
    void stop();

    bool addEvent(socket_t fd, EventType type, EventHandler handler);
    bool modifyEvent(socket_t fd, EventType type);
    bool removeEvent(socket_t fd);

private:
    std::atomic<bool> running_{false};
    
#ifdef _WIN32
    // Windows implementation using select
    struct EventContext {
        socket_t fd;
        EventType type;
        EventHandler handler;
    };
    std::vector<EventContext> events_;
    std::mutex events_mutex_;
#else
    // Linux implementation using epoll
    int epoll_fd_{INVALID_SOCKET};
    struct EventHandlerContext {
        EventHandler handler;
    };
    std::vector<EventHandlerContext*> handlers_;
#endif
};

} // namespace network
} // namespace inferno

#endif // INFERNOCACHE_EVENT_LOOP_H
