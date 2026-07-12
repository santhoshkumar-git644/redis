# Milestone 11: Single-Threaded Reactor Pattern Design

## Overview
One of Redis's most famous architectural characteristics is that it is primarily **single-threaded**. Despite utilizing only one CPU core, a well-tuned Redis instance can serve millions of operations per second. 
Milestone 11 officially migrates InfernoCache into a true single-threaded Reactor Pattern, perfectly mimicking this architecture.

## Why Single-Threaded?
In traditional networking (e.g., Apache HTTP Server or older databases), the server spawns one OS thread (or process) per connected client.
1. **Thread Overhead**: Context switching between 10,000 threads destroys CPU cache lines and wastes cycles.
2. **Locking**: Mutating shared memory (like a global Hash Table) from multiple threads requires aggressive locking (Mutexes/Spinlocks). This serialization introduces massive latency spikes under contention.

By using a single thread, InfernoCache entirely bypasses the need for Mutexes on its core dictionary. Execution is lock-free, strictly sequential, and highly predictable.

## The Reactor Pattern (Event Loop)
Instead of blocking on socket reads, we use non-blocking I/O and an OS-level I/O multiplexer:
- **Linux**: `epoll`
- **Windows**: `select` (or `WSAPoll`)
- **macOS/BSD**: `kqueue`

The `EventLoop` acts as the "Reactor". It asks the OS: *"Which of these 10,000 sockets has data ready to read right now?"* 
The OS immediately returns an array of ready file descriptors. The EventLoop iterates through them, parses the RESP commands, executes them against the `Dict`, and writes the response. Because operations are in-memory (O(1) or O(log N)), they execute in nanoseconds, never blocking the loop long enough to starve other clients.

## Timer Events & Active Expiration
Previously, active key expiration (randomly sampling and deleting expired keys) ran in a detached `std::thread` using `sleep_for(100ms)`. This violated the single-threaded philosophy and required `std::shared_mutex` on the dictionary.

In Milestone 11, the `EventLoop` was enhanced to support **Timer Events**. 
- The loop calculates the time remaining until the next scheduled timer (e.g., Active Expiration).
- It passes this delta as the timeout to `epoll_wait` or `select`.
- If no network traffic arrives before the timeout, the loop wakes up and executes the Timer Handler.

Now, Active Expiration runs seamlessly *inside* the main thread, perfectly interleaved with client commands, eliminating background data structure mutation. The only background threads remaining are for strict persistence I/O (`fsync` and `BGSAVE`), which operate entirely on immutable snapshots or append-only file handles.
