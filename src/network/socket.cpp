#include "socket.h"
#include "../utils/logger.h"
#include <stdexcept>
#include <cstring>

namespace inferno {
namespace network {

void Socket::initializeNetwork() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LOG_ERROR("WSAStartup failed");
        throw std::runtime_error("Network initialization failed");
    }
#endif
}

void Socket::cleanupNetwork() {
#ifdef _WIN32
    WSACleanup();
#endif
}

Socket::Socket() : fd_(INVALID_SOCKET) {
    fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd_ == INVALID_SOCKET) {
        LOG_ERROR("Failed to create socket");
    }
}

Socket::Socket(socket_t fd) : fd_(fd) {}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = INVALID_SOCKET;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = INVALID_SOCKET;
    }
    return *this;
}

Socket::~Socket() {
    close();
}

void Socket::close() {
    if (fd_ != INVALID_SOCKET) {
#ifdef _WIN32
        ::closesocket(fd_);
#else
        ::close(fd_);
#endif
        fd_ = INVALID_SOCKET;
    }
}

bool Socket::bind(const std::string& ip, int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    
    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid address/ Address not supported: " + ip);
        return false;
    }

    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        LOG_ERROR("Bind failed");
        return false;
    }
    return true;
}

bool Socket::listen(int backlog) {
    if (::listen(fd_, backlog) == SOCKET_ERROR) {
        LOG_ERROR("Listen failed");
        return false;
    }
    return true;
}

Socket Socket::accept() {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    
    socket_t client_fd = ::accept(fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
    if (client_fd == INVALID_SOCKET) {
        // Not necessarily an error if non-blocking
        return Socket(INVALID_SOCKET);
    }
    return Socket(client_fd);
}

bool Socket::connect(const std::string& ip, int port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    
    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid address for connect: " + ip);
        return false;
    }

    if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        return false;
    }
    return true;
}

int Socket::send(const char* data, size_t length) {
    int sent = ::send(fd_, data, static_cast<int>(length), 0);
    return sent;
}

int Socket::receive(char* buffer, size_t length) {
    int received = ::recv(fd_, buffer, static_cast<int>(length), 0);
    return received;
}

bool Socket::setNonBlocking(bool non_blocking) {
#ifdef _WIN32
    u_long mode = non_blocking ? 1 : 0;
    return ioctlsocket(fd_, FIONBIO, &mode) != SOCKET_ERROR;
#else
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags == -1) return false;
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return ::fcntl(fd_, F_SETFL, flags) != -1;
#endif
}

bool Socket::setReuseAddress(bool reuse) {
    int opt = reuse ? 1 : 0;
    return ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) != SOCKET_ERROR;
}

bool Socket::setTcpNoDelay(bool no_delay) {
    int opt = no_delay ? 1 : 0;
    return ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&opt), sizeof(opt)) != SOCKET_ERROR;
}

} // namespace network
} // namespace inferno
