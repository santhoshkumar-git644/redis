#include <gtest/gtest.h>
#include "../src/network/tcp_server.h"
#include "../src/network/socket.h"
#include <thread>
#include <chrono>

using namespace inferno::network;

class NetworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Find an open port (using 0 usually assigns an ephemeral port, 
        // but for simplicity we use a fixed high port)
        test_port = 16379;
        
        server_thread = std::thread([this]() {
            server = std::make_unique<TCPServer>("127.0.0.1", test_port);
            server->start();
        });
        
        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        if (server) {
            server->stop();
        }
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }

    int test_port;
    std::unique_ptr<TCPServer> server;
    std::thread server_thread;
};

TEST_F(NetworkTest, ClientConnectionAndEcho) {
    Socket::initializeNetwork();

    Socket client_socket;
    bool connected = client_socket.connect("127.0.0.1", test_port);
    EXPECT_TRUE(connected);

    std::string msg = "PING";
    int sent = client_socket.send(msg.c_str(), msg.length());
    EXPECT_EQ(sent, msg.length());

    char buffer[1024] = {0};
    int received = client_socket.receive(buffer, sizeof(buffer));
    
    // In our milestone 1 server, it echoes the data back
    EXPECT_EQ(received, msg.length());
    EXPECT_STREQ(buffer, "PING");

    client_socket.close();
    Socket::cleanupNetwork();
}
