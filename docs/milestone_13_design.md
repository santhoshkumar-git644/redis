# Milestone 13: Publish/Subscribe (Pub/Sub) Design

## Overview
Up until this point, InfernoCache has strictly adhered to a Request-Response model: a client sends a command, and the server computes and sends back a response.
Milestone 13 breaks this paradigm by introducing the **Publish/Subscribe (Pub/Sub) System**, enabling asynchronous message broadcasting and server-push architectures.

## Architecture

1. **`PubSubManager` (The Routing Engine)**
   - To decouple the core dictionary storage from the network routing, a dedicated `PubSubManager` singleton was introduced.
   - It maintains two critical bidirectional mappings:
     1. `channels_`: `std::unordered_map<std::string, std::unordered_set<network::socket_t>>`
        - Maps a channel name (e.g., `"news"`) to the socket file descriptors (FDs) of all subscribed clients.
        - Used during `PUBLISH` to find all targets in O(1) time.
     2. `client_channels_`: `std::unordered_map<network::socket_t, std::unordered_set<std::string>>`
        - Maps a client's socket FD to all the channels they are subscribed to.
        - Used during client disconnect to instantly remove them from all channels without having to scan the entire `channels_` routing table (O(K) time where K is their subscription count).

2. **Integration with `CommandHandler` & `TCPServer`**
   - The `CommandHandler::handleCommand` method was fundamentally upgraded. It now natively accepts the `client_fd` of the client issuing the command.
   - This allows commands like `SUBSCRIBE` to inform the `PubSubManager` *who* is subscribing.
   - To support asynchronous server-push, `TCPServer` injects a `SendCallback` directly into the `PubSubManager`. When `PUBLISH` is called, the manager iterates over the target socket FDs and pushes raw RESP arrays straight into their TCP streams, bypassing the standard request-response lifecycle!

3. **Commands Implemented**
   - `SUBSCRIBE channel [channel ...]`: Subscribes the client to the specified channels. Returns confirmation arrays.
   - `UNSUBSCRIBE [channel ...]`: Unsubscribes from the specified channels.
   - `PUBLISH channel message`: Posts a message to the given channel. Returns the number of clients that received the message.

## Memory & Performance
- Because Pub/Sub operates entirely in the network routing layer and does not touch the `Dict` storage engine, published messages are strictly ephemeral. If a client is disconnected during a `PUBLISH`, they miss the message forever.
- The use of `std::unordered_set` ensures that adding, removing, and looking up subscribers occurs in O(1) amortized time, allowing InfernoCache to easily route millions of messages per second across thousands of connected clients.
