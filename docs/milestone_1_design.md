# Milestone 1: Networking Foundation

## Overview
This milestone establishes the foundational networking layer of InfernoCache. The core responsibility of this layer is to handle multiple concurrent TCP client connections asynchronously without busy-waiting.

## Architecture

### Components
1. **Socket**: RAII abstraction over native OS socket descriptors. Ensures sockets are properly configured (non-blocking, TCP_NODELAY) and closed securely.
2. **EventLoop**: Cross-platform event polling mechanism. On Linux, this will use `epoll` for high-performance edge-triggered or level-triggered polling. On Windows, it will fallback to `select()` or `WSAPoll()` (as `epoll` is Linux-specific) to maintain cross-platform support for development.
3. **ConnectionManager**: Manages the lifecycle of active client connections, tracking them by file descriptor.
4. **TCPServer**: The main entry point for network I/O. It binds to a specific port, listens for incoming connections, accepts them, and registers them with the `EventLoop`. It also routes incoming data events to the respective handlers.

### Rationale
Redis uses a single-threaded event loop driven by `epoll` (or `kqueue` on macOS). This design inherently avoids the complexity of multithreading locks for data structures, making operations deterministic and atomic.

Our design mirrors this conceptually. For Milestone 1, we implement an `EventLoop` that multiplexes I/O, allowing a single thread to handle thousands of concurrent connections efficiently.

### Complexity
- **Time Complexity:** 
  - Accepting a connection: `O(1)`
  - Polling for events: `O(N)` where N is the number of triggered events (for `epoll`) rather than the number of total connections (as in `select`).
- **Memory Complexity:** 
  - `O(C)` where C is the number of active connections (each requires an entry in the ConnectionManager and underlying socket buffers).
