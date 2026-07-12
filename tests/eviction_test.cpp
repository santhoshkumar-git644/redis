#include <gtest/gtest.h>
#include "../src/server/command_handler.h"
#include "../src/storage/dict.h"
#include <thread>
#include <chrono>

using namespace inferno::protocol;
using namespace inferno::server;
using namespace inferno::storage;

class EvictionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear global dict for a fresh start
        auto& handler = CommandHandler::instance();
        handler.getDict().setMaxKeys(0);
        handler.getDict().setEvictionPolicy(EvictionPolicy::NOEVICTION);
        
        auto cmd = std::make_shared<RESPArray>();
        cmd->elements = { RESPBulkString("DEL"), RESPBulkString("k1"), RESPBulkString("k2"), RESPBulkString("k3"), RESPBulkString("k4") };
        handler.handleCommand(cmd);
    }
    
    void TearDown() override {
        auto& handler = CommandHandler::instance();
        handler.getDict().setMaxKeys(0);
        handler.getDict().setEvictionPolicy(EvictionPolicy::NOEVICTION);
    }
};

TEST_F(EvictionTest, LRUEviction) {
    auto& handler = CommandHandler::instance();
    auto& dict = handler.getDict();
    
    // Set max keys to 3, with LRU policy
    dict.setEvictionPolicy(EvictionPolicy::ALLKEYS_LRU);
    dict.setMaxKeys(3);
    
    // Add 3 keys
    auto set_cmd1 = std::make_shared<RESPArray>();
    set_cmd1->elements = { RESPBulkString("SET"), RESPBulkString("k1"), RESPBulkString("v1") };
    handler.handleCommand(set_cmd1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    auto set_cmd2 = std::make_shared<RESPArray>();
    set_cmd2->elements = { RESPBulkString("SET"), RESPBulkString("k2"), RESPBulkString("v2") };
    handler.handleCommand(set_cmd2);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    auto set_cmd3 = std::make_shared<RESPArray>();
    set_cmd3->elements = { RESPBulkString("SET"), RESPBulkString("k3"), RESPBulkString("v3") };
    handler.handleCommand(set_cmd3);
    
    // Now we access k1 and k2 so that k3 is the least recently used
    auto get_cmd1 = std::make_shared<RESPArray>();
    get_cmd1->elements = { RESPBulkString("GET"), RESPBulkString("k1") };
    handler.handleCommand(get_cmd1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    auto get_cmd2 = std::make_shared<RESPArray>();
    get_cmd2->elements = { RESPBulkString("GET"), RESPBulkString("k2") };
    handler.handleCommand(get_cmd2);
    
    // Add 4th key, which should evict k3 (since it hasn't been accessed)
    auto set_cmd4 = std::make_shared<RESPArray>();
    set_cmd4->elements = { RESPBulkString("SET"), RESPBulkString("k4"), RESPBulkString("v4") };
    handler.handleCommand(set_cmd4);
    
    // k4, k2, k1 should exist. k3 should be evicted (because our sampling is random, 
    // it's an approximation, but with only 3 keys, it's basically guaranteed to sample k3).
    // Actually, sampling 5 from a pool of 3 means it checks all of them.
    EXPECT_TRUE(dict.exists("k4"));
    EXPECT_TRUE(dict.exists("k2"));
    EXPECT_TRUE(dict.exists("k1"));
    EXPECT_FALSE(dict.exists("k3"));
}

TEST_F(EvictionTest, LFUEviction) {
    auto& handler = CommandHandler::instance();
    auto& dict = handler.getDict();
    
    dict.setEvictionPolicy(EvictionPolicy::ALLKEYS_LFU);
    dict.setMaxKeys(3);
    
    // Add 3 keys
    auto set_cmd1 = std::make_shared<RESPArray>();
    set_cmd1->elements = { RESPBulkString("SET"), RESPBulkString("k1"), RESPBulkString("v1") };
    handler.handleCommand(set_cmd1);
    
    auto set_cmd2 = std::make_shared<RESPArray>();
    set_cmd2->elements = { RESPBulkString("SET"), RESPBulkString("k2"), RESPBulkString("v2") };
    handler.handleCommand(set_cmd2);
    
    auto set_cmd3 = std::make_shared<RESPArray>();
    set_cmd3->elements = { RESPBulkString("SET"), RESPBulkString("k3"), RESPBulkString("v3") };
    handler.handleCommand(set_cmd3);
    
    // Access k1 5 times
    auto get_cmd1 = std::make_shared<RESPArray>();
    get_cmd1->elements = { RESPBulkString("GET"), RESPBulkString("k1") };
    for (int i=0; i<5; ++i) handler.handleCommand(get_cmd1);
    
    // Access k3 3 times
    auto get_cmd3 = std::make_shared<RESPArray>();
    get_cmd3->elements = { RESPBulkString("GET"), RESPBulkString("k3") };
    for (int i=0; i<3; ++i) handler.handleCommand(get_cmd3);
    
    // k2 has only 1 access (from SET). It is the Least Frequently Used.
    
    // Add 4th key
    auto set_cmd4 = std::make_shared<RESPArray>();
    set_cmd4->elements = { RESPBulkString("SET"), RESPBulkString("k4"), RESPBulkString("v4") };
    handler.handleCommand(set_cmd4);
    
    // Verify k2 was evicted
    EXPECT_TRUE(dict.exists("k4"));
    EXPECT_TRUE(dict.exists("k1"));
    EXPECT_TRUE(dict.exists("k3"));
    EXPECT_FALSE(dict.exists("k2"));
}
