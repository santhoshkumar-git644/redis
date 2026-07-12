# Milestone 2: RESP Protocol Design

## Overview
This milestone introduces the REdis Serialization Protocol (RESP2) parser to InfernoCache. RESP is the standard protocol used by Redis clients to communicate with the server.

## Architecture

### Components
1. **`RESPObject`**: A sum type (`std::variant`) representing the five data types of RESP2:
   - Simple Strings
   - Errors
   - Integers
   - Bulk Strings
   - Arrays
2. **`RESPParser`**: A state-machine-based incremental parser.

### Incremental Parsing Rationale
TCP is a streaming protocol. A single call to `recv()` might yield half a command, or two and a half commands. A naive parser would wait for a newline or assume full commands, which breaks under real network conditions or pipeline usage.

Our `RESPParser` accumulates bytes in an internal buffer and processes them incrementally. If a payload is incomplete (e.g., waiting for the rest of a bulk string, or waiting for all elements of an array), it safely returns `NEED_MORE` and preserves its state. 

When the next chunk of data arrives from the `EventLoop`, the parser resumes exactly where it left off.

### Time & Memory Complexity
- **Time Complexity**: 
  - Parsing a Simple String / Integer: `O(N)` where N is the length of the line.
  - Parsing a Bulk String: `O(L)` where L is the payload length. 
  - Overall time complexity is `O(B)` where B is the number of bytes read. We avoid excessive string copying by consuming directly from the buffer.
- **Memory Complexity**: 
  - The internal buffer holds at most the size of the unparsed data chunk.
  - The `std::variant` object model is extremely lightweight. Bulk strings do require allocating `std::string`, which is standard. For extreme optimizations (like Redis), zero-copy parsing could be introduced later.

### Comparison to Redis
Redis implements its parser in `networking.c`. It also uses a state machine (`reqtype`, `multibulklen`, `bulklen`, etc.). By using modern C++ (`std::variant`, `std::stack`), we achieve the same robust behavior with significantly cleaner code than traditional C.
