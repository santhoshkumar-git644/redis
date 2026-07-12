#include <gtest/gtest.h>
#include "../src/server/command_handler.h"
#include "../src/persistence/aof.h"
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>

using namespace inferno::protocol;
using namespace inferno::server;
using namespace inferno::persistence;

class AOFTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_filename = "test_inferno.aof";
        std::remove(test_filename.c_str());
        // Clear global dict for a fresh start
        // Normally we'd want a clean CommandHandler instance, but it's a singleton.
        // Let's just manually delete the keys we use.
        auto cmd = std::make_shared<RESPArray>();
        cmd->elements = { RESPBulkString("DEL"), RESPBulkString("aof_key1"), RESPBulkString("aof_key2") };
        CommandHandler::instance().handleCommand(cmd);
    }
    
    void TearDown() override {
        AOFManager::instance().stop();
        std::remove(test_filename.c_str());
    }

    std::string test_filename;
};

TEST_F(AOFTest, SerializeAndAppend) {
    auto& aof = AOFManager::instance();
    aof.start(test_filename, AOFFsyncPolicy::ALWAYS); // ALWAYS to avoid timing issues in test
    
    auto cmd1 = std::make_shared<RESPArray>();
    cmd1->elements = { RESPBulkString("SET"), RESPBulkString("aof_key1"), RESPBulkString("hello") };
    
    // Test serialization
    std::string expected = "*3\r\n$3\r\nSET\r\n$8\r\naof_key1\r\n$5\r\nhello\r\n";
    EXPECT_EQ(aof.serialize(cmd1), expected);
    
    // Let's go through CommandHandler to test hooking
    CommandHandler::instance().handleCommand(cmd1);
    
    // Stop AOF to flush and close
    aof.stop();
    
    // Read file and verify
    std::ifstream file(test_filename, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, expected);
}

TEST_F(AOFTest, Recovery) {
    auto& aof = AOFManager::instance();
    
    // 1. Write some commands
    aof.start(test_filename, AOFFsyncPolicy::ALWAYS);
    
    auto cmd1 = std::make_shared<RESPArray>();
    cmd1->elements = { RESPBulkString("SET"), RESPBulkString("aof_key2"), RESPBulkString("world") };
    CommandHandler::instance().handleCommand(cmd1);
    
    auto cmd2 = std::make_shared<RESPArray>();
    cmd2->elements = { RESPBulkString("INCR"), RESPBulkString("aof_key3") };
    CommandHandler::instance().handleCommand(cmd2);
    
    aof.stop();
    
    // 2. Clear dict to simulate restart
    auto del_cmd = std::make_shared<RESPArray>();
    del_cmd->elements = { RESPBulkString("DEL"), RESPBulkString("aof_key2"), RESPBulkString("aof_key3") };
    CommandHandler::instance().handleCommand(del_cmd);
    
    // 3. Recover
    aof.start(test_filename, AOFFsyncPolicy::NO); // Just sets filename
    aof.stop(); // To ensure it's not holding file lock
    aof.start(test_filename, AOFFsyncPolicy::NO); // Just setting up filename before load isn't strictly necessary if we stopped it, but start() opens it for append. Wait, load() opens in read.
    aof.stop(); // we just need filename_ set inside instance. Actually, AOFManager has no way to just set filename without start(). Let's just start then stop to set filename, then load? No, start opens for append.
    // Let's just rely on the previous start setting the filename internally.
    
    EXPECT_TRUE(aof.load());
    
    // 4. Verify state was recovered
    auto get_cmd = std::make_shared<RESPArray>();
    get_cmd->elements = { RESPBulkString("GET"), RESPBulkString("aof_key2") };
    auto res1 = CommandHandler::instance().handleCommand(get_cmd);
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(res1));
    EXPECT_EQ(std::get<RESPBulkString>(res1), "world");
    
    auto get_cmd2 = std::make_shared<RESPArray>();
    get_cmd2->elements = { RESPBulkString("GET"), RESPBulkString("aof_key3") };
    auto res2 = CommandHandler::instance().handleCommand(get_cmd2);
    EXPECT_EQ(std::get<RESPBulkString>(res2), "1");
}
