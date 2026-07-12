#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

// A simple utility to benchmark the pipelining throughput of InfernoCache
int main(int argc, char* argv[]) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    std::string host = "127.0.0.1";
    int port = 6379;
    
    if (argc > 1) host = argv[1];
    if (argc > 2) port = std::stoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed to " << host << ":" << port << "\n";
        return 1;
    }

    std::cout << "Connected to InfernoCache at " << host << ":" << port << "\n";

    const int TOTAL_REQUESTS = 1000000;
    const int PIPELINE_BATCH_SIZE = 10000;
    
    std::string pipeline_payload;
    pipeline_payload.reserve(PIPELINE_BATCH_SIZE * 50); // Preallocate
    
    for (int i = 0; i < PIPELINE_BATCH_SIZE; ++i) {
        pipeline_payload += "*3\r\n$3\r\nSET\r\n$10\r\nbench_key_\r\n$9\r\nbench_val\r\n";
    }

    std::cout << "Starting benchmark: " << TOTAL_REQUESTS << " SET commands...\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    int requests_sent = 0;
    int requests_received = 0;

    std::vector<char> rx_buffer(1024 * 1024); // 1MB receive buffer

    while (requests_received < TOTAL_REQUESTS) {
        // Send a batch if we haven't sent everything yet
        if (requests_sent < TOTAL_REQUESTS) {
            int to_send = std::min(PIPELINE_BATCH_SIZE, TOTAL_REQUESTS - requests_sent);
            // We just send the pre-computed payload (assuming PIPELINE_BATCH_SIZE divides TOTAL_REQUESTS)
            send(sock, pipeline_payload.c_str(), pipeline_payload.length(), 0);
            requests_sent += to_send;
        }

        // Receive responses
        int bytes_read = recv(sock, rx_buffer.data(), rx_buffer.size(), 0);
        if (bytes_read <= 0) {
            std::cerr << "Connection closed by server prematurely.\n";
            break;
        }

        // Count how many '+OK\r\n' we got
        for (int i = 0; i < bytes_read - 2; ++i) {
            if (rx_buffer[i] == '+' && rx_buffer[i+1] == 'O' && rx_buffer[i+2] == 'K') {
                requests_received++;
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end_time - start_time;
    
    std::cout << "\n--- Benchmark Results ---\n";
    std::cout << "Total Requests: " << requests_received << "\n";
    std::cout << "Total Time: " << diff.count() << " seconds\n";
    std::cout << "Throughput: " << static_cast<int>(requests_received / diff.count()) << " requests/second\n";

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    return 0;
}
