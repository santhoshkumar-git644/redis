# Milestone 16: Pipelining Design

## Overview
In standard Request-Response interactions, a client sends a command, waits for the response to traverse the network, and then sends the next command. 
This introduces severe latency bottlenecking driven by network Round-Trip Time (RTT).

**Pipelining** allows clients to aggressively blast hundreds or thousands of commands to the server sequentially without waiting for individual responses.
InfernoCache natively handles Pipelining out of the box because of the `RESPParser` architecture introduced in Milestone 2, but in Milestone 16, we heavily optimized the underlying syscalls to increase server throughput dramatically.

## Architecture

1. **Continuous Parsing Buffer**
   - The `RESPParser` (built in Milestone 2) is designed as a state machine. It does not assume that a network packet perfectly correlates to a single command.
   - When a client pipelines 1,000 commands, they might arrive at the server in 3 massive chunks of bytes.
   - The parser's `feed()` method consumes these bytes into an internal stream. The `parse()` method is then called in a tight `while(true)` loop.
   - `parse()` will successfully construct and yield command after command until the stream runs dry, at which point it yields `NEED_MORE` and the server waits for the next network chunk.

2. **The O(1) Syscall Optimization (Milestone 16)**
   - Prior to Milestone 16, every time a command was parsed and executed, `TCPServer` instantly called `socket.send()` to push the response string back to the client.
   - If a client pipelined 10,000 commands, the server would execute 10,000 `send()` syscalls. Syscalls are incredibly expensive because they require a context switch from user space into kernel space.
   - **The Fix:** We introduced an `out_buffer` string in the `handleClientData` loop.
   - Instead of calling `send()` for every command, we simply append the `serializeRESP()` string to the `out_buffer`.
   - Once the parser loop completes (meaning we have drained all currently available bytes from the socket), we call `send()` exactly **once**, transmitting the massive, batched response payload back to the client in a single O(1) syscall.

## Performance Impact
This singular change dramatically increases the requests-per-second (RPS) capacity of InfernoCache during high-throughput workloads, mirroring the exact network buffering strategies used by enterprise data systems to maximize hardware utilization.
