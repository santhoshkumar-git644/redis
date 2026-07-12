#include "event_loop.h"
#include "../utils/logger.h"
#include <algorithm>

#ifdef _WIN32
// Windows implementation using select
#else
#include <sys/epoll.h>
#endif

namespace inferno {
namespace network {

EventLoop::EventLoop() {}

EventLoop::~EventLoop() {
    stop();
#ifndef _WIN32
    if (epoll_fd_ != INVALID_SOCKET) {
        ::close(epoll_fd_);
    }
    for (auto ctx : handlers_) {
        delete ctx;
    }
#endif
}

bool EventLoop::initialize() {
#ifdef _WIN32
    // Nothing specific to initialize for select
    return true;
#else
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        LOG_ERROR("Failed to create epoll instance");
        return false;
    }
    return true;
#endif
}

void EventLoop::run() {
    running_ = true;
    LOG_INFO("Event loop started");

#ifdef _WIN32
    while (running_) {
        fd_set read_fds;
        fd_set write_fds;
        fd_set error_fds;
        
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&error_fds);
        
        socket_t max_fd = 0;
        
        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            for (const auto& ctx : events_) {
                if (ctx.type == EventType::READ) {
                    FD_SET(ctx.fd, &read_fds);
                } else if (ctx.type == EventType::WRITE) {
                    FD_SET(ctx.fd, &write_fds);
                }
                FD_SET(ctx.fd, &error_fds);
                if (ctx.fd > max_fd) {
                    max_fd = ctx.fd;
                }
            }
        }
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout
        
        int activity = select(static_cast<int>(max_fd + 1), &read_fds, &write_fds, &error_fds, &tv);
        
        if (activity < 0) {
            LOG_ERROR("select error");
            break;
        } else if (activity == 0) {
            continue; // Timeout
        }
        
        std::vector<EventContext> current_events;
        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            current_events = events_;
        }
        
        for (const auto& ctx : current_events) {
            if (FD_ISSET(ctx.fd, &error_fds)) {
                ctx.handler(ctx.fd, EventType::ERROR);
            } else if (ctx.type == EventType::READ && FD_ISSET(ctx.fd, &read_fds)) {
                ctx.handler(ctx.fd, EventType::READ);
            } else if (ctx.type == EventType::WRITE && FD_ISSET(ctx.fd, &write_fds)) {
                ctx.handler(ctx.fd, EventType::WRITE);
            }
        }
    }
#else
    constexpr int MAX_EVENTS = 1024;
    struct epoll_event events[MAX_EVENTS];

    while (running_) {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);
        
        if (nfds == -1) {
            if (errno == EINTR) continue;
            LOG_ERROR("epoll_wait error");
            break;
        }
        
        for (int i = 0; i < nfds; ++i) {
            EventHandlerContext* ctx = static_cast<EventHandlerContext*>(events[i].data.ptr);
            socket_t fd = 0; // In a full implementation, we'd map this back, but handler captures it typically
            
            if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP) {
                ctx->handler(fd, EventType::ERROR);
            } else if (events[i].events & EPOLLIN) {
                ctx->handler(fd, EventType::READ);
            } else if (events[i].events & EPOLLOUT) {
                ctx->handler(fd, EventType::WRITE);
            }
        }
    }
#endif
    LOG_INFO("Event loop stopped");
}

void EventLoop::stop() {
    running_ = false;
}

bool EventLoop::addEvent(socket_t fd, EventType type, EventHandler handler) {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(events_mutex_);
    events_.push_back({fd, type, handler});
    return true;
#else
    struct epoll_event event;
    event.events = (type == EventType::READ) ? EPOLLIN : EPOLLOUT;
    
    EventHandlerContext* ctx = new EventHandlerContext{handler};
    event.data.ptr = ctx;
    handlers_.push_back(ctx); // simplistic memory management for milestone 1
    
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) == -1) {
        LOG_ERROR("epoll_ctl ADD failed");
        return false;
    }
    return true;
#endif
}

bool EventLoop::modifyEvent(socket_t fd, EventType type) {
    // Basic implementation for milestone 1
    return true;
}

bool EventLoop::removeEvent(socket_t fd) {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(events_mutex_);
    events_.erase(
        std::remove_if(events_.begin(), events_.end(),
            [fd](const EventContext& ctx) { return ctx.fd == fd; }),
        events_.end());
    return true;
#else
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        LOG_ERROR("epoll_ctl DEL failed");
        return false;
    }
    return true;
#endif
}

} // namespace network
} // namespace inferno
