#ifndef INFERNOCACHE_SOCKET_H
#define INFERNOCACHE_SOCKET_H

#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
using socket_t = int;
constexpr int INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
#endif

namespace inferno {
namespace network {

class Socket {
public:
    // Create a new socket
    Socket();
    
    // Wrap an existing socket descriptor (e.g., from accept)
    explicit Socket(socket_t fd);
    
    // Non-copyable
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    
    // Movable
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    
    ~Socket();

    // Core operations
    bool bind(const std::string& ip, int port);
    bool listen(int backlog = 128);
    Socket accept();
    
    bool connect(const std::string& ip, int port);
    
    int send(const char* data, size_t length);
    int receive(char* buffer, size_t length);
    
    // Configuration
    bool setNonBlocking(bool non_blocking = true);
    bool setReuseAddress(bool reuse = true);
    bool setTcpNoDelay(bool no_delay = true);

    void close();
    bool isValid() const { return fd_ != INVALID_SOCKET; }
    socket_t getFd() const { return fd_; }

    static void initializeNetwork();
    static void cleanupNetwork();

private:
    socket_t fd_;
};

} // namespace network
} // namespace inferno

#endif // INFERNOCACHE_SOCKET_H
