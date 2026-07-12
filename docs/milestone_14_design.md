# Milestone 14: Transactions Design

## Overview
In a highly concurrent system, clients often need to execute a series of operations without the risk of another client modifying the dataset in the middle. 
Milestone 14 introduces **Transactions (`MULTI`, `EXEC`, `DISCARD`)**, giving clients the ability to queue commands and execute them atomically.

## Architecture

1. **Per-Client State**
   - The `CommandHandler` maintains two state structures for transactions:
     - `in_transaction_` (`std::unordered_set<socket_t>`): Tracks which clients are currently in `MULTI` mode.
     - `transaction_queues_` (`std::unordered_map<socket_t, std::vector<RESPArray>>`): Holds the buffered commands for each client.

2. **Command Interception**
   - When a client is in `in_transaction_` mode, the `CommandHandler::handleCommand` method intercepts all incoming commands (except `MULTI`, `EXEC`, `DISCARD`, and `QUIT`).
   - Instead of dispatching them immediately, it pushes the raw `RESPArray` command object to the client's queue and returns `+QUEUED\r\n`.

3. **Atomic Execution (`EXEC`)**
   - When the client issues `EXEC`, the `CommandHandler` retrieves the queue.
   - It iterates through the buffered commands and explicitly bypasses the queuing interceptor to dispatch them.
   - It collects all the resulting `RESPObject`s into a single `RESPArray` and returns the bulk payload.
   
## The Atomicity Guarantee (The Reactor Pattern)
The most elegant aspect of this implementation is that **we did not have to write a single Mutex or thread lock to guarantee atomicity.**

Because InfernoCache operates on a single-threaded **Reactor Pattern** (`EventLoop`), only one client's commands can be processed at any exact moment in time. When `EXEC` is called, the thread begins iterating through the client's queue in a tight `for` loop.

Since the thread never yields or blocks on I/O during this loop, it is physically impossible for the `EventLoop` to parse or execute commands from *another* client's socket while the transaction is executing. 
The transaction is inherently atomic, exactly mirroring real Redis architecture!
