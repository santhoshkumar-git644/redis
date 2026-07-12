#include <gtest/gtest.h>
#include "../src/server/command_handler.h"
#include "../src/persistence/rdb.h"
#include <cstdio>
#include <thread>
#include <chrono>

using namespace inferno::protocol;
using namespace inferno::server;
using namespace inferno::persistence;

class RDBTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_filename = "test_inferno.rdb";
        std::remove(test_filename.c_str());
        
        // Clear dict
        auto cmd = std::make_shared<RESPArray>();
        cmd->elements = { RESPBulkString("DEL"), 
                          RESPBulkString("rdb_string"), 
                          RESPBulkString("rdb_list"),
                          RESPBulkString("rdb_set"),
                          RESPBulkString("rdb_hash"),
                          RESPBulkString("rdb_zset") };
        CommandHandler::instance().handleCommand(cmd);
    }
    
    void TearDown() override {
        std::remove(test_filename.c_str());
    }

    std::string test_filename;
};

TEST_F(RDBTest, SerializeAllTypes) {
    auto& handler = CommandHandler::instance();
    auto& rdb = RDBManager::instance();
    
    // 1. String (INT encoding)
    auto set_cmd = std::make_shared<RESPArray>();
    set_cmd->elements = { RESPBulkString("SET"), RESPBulkString("rdb_string"), RESPBulkString("12345") };
    handler.handleCommand(set_cmd);
    
    // 2. List
    auto lpush_cmd = std::make_shared<RESPArray>();
    lpush_cmd->elements = { RESPBulkString("LPUSH"), RESPBulkString("rdb_list"), RESPBulkString("list_val") };
    handler.handleCommand(lpush_cmd);
    
    // 3. Set
    auto sadd_cmd = std::make_shared<RESPArray>();
    sadd_cmd->elements = { RESPBulkString("SADD"), RESPBulkString("rdb_set"), RESPBulkString("set_val") };
    handler.handleCommand(sadd_cmd);
    
    // 4. Hash
    auto hset_cmd = std::make_shared<RESPArray>();
    hset_cmd->elements = { RESPBulkString("HSET"), RESPBulkString("rdb_hash"), RESPBulkString("field1"), RESPBulkString("val1") };
    handler.handleCommand(hset_cmd);
    
    // 5. ZSet
    auto zadd_cmd = std::make_shared<RESPArray>();
    zadd_cmd->elements = { RESPBulkString("ZADD"), RESPBulkString("rdb_zset"), RESPBulkString("3.14"), RESPBulkString("pi") };
    handler.handleCommand(zadd_cmd);
    
    // 6. Expiry
    auto expire_cmd = std::make_shared<RESPArray>();
    expire_cmd->elements = { RESPBulkString("EXPIRE"), RESPBulkString("rdb_string"), RESPBulkString("1000") }; // 1000 seconds
    handler.handleCommand(expire_cmd);
    
    // Perform Synchronous Save
    EXPECT_TRUE(rdb.save(handler.getDict(), test_filename));
    
    // Clear the memory
    auto del_cmd = std::make_shared<RESPArray>();
    del_cmd->elements = { RESPBulkString("DEL"), 
                          RESPBulkString("rdb_string"), 
                          RESPBulkString("rdb_list"),
                          RESPBulkString("rdb_set"),
                          RESPBulkString("rdb_hash"),
                          RESPBulkString("rdb_zset") };
    handler.handleCommand(del_cmd);
    
    // Load the RDB
    EXPECT_TRUE(rdb.load(handler.getDict(), test_filename));
    
    // Verify String
    auto get_cmd = std::make_shared<RESPArray>();
    get_cmd->elements = { RESPBulkString("GET"), RESPBulkString("rdb_string") };
    auto res1 = handler.handleCommand(get_cmd);
    EXPECT_EQ(std::get<RESPBulkString>(res1), "12345");
    
    // Verify TTL
    auto ttl_cmd = std::make_shared<RESPArray>();
    ttl_cmd->elements = { RESPBulkString("TTL"), RESPBulkString("rdb_string") };
    auto ttl_res = handler.handleCommand(ttl_cmd);
    EXPECT_TRUE(std::holds_alternative<RESPInteger>(ttl_res));
    EXPECT_GT(std::get<RESPInteger>(ttl_res), 0); // Still active
    
    // Verify ZSet
    auto zscore_cmd = std::make_shared<RESPArray>();
    zscore_cmd->elements = { RESPBulkString("ZSCORE"), RESPBulkString("rdb_zset"), RESPBulkString("pi") };
    auto zscore_res = handler.handleCommand(zscore_cmd);
    EXPECT_EQ(std::stod(std::get<RESPBulkString>(zscore_res)), 3.14);
}

TEST_F(RDBTest, BackgroundSave) {
    auto& handler = CommandHandler::instance();
    auto& rdb = RDBManager::instance();
    
    auto set_cmd = std::make_shared<RESPArray>();
    set_cmd->elements = { RESPBulkString("SET"), RESPBulkString("rdb_bg"), RESPBulkString("bg_val") };
    handler.handleCommand(set_cmd);
    
    // BGSAVE
    EXPECT_TRUE(rdb.bgsave(handler.getDict(), test_filename));
    
    // Wait for bgsave to finish
    while (rdb.isBgsaveInProgress()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Clear memory
    auto del_cmd = std::make_shared<RESPArray>();
    del_cmd->elements = { RESPBulkString("DEL"), RESPBulkString("rdb_bg") };
    handler.handleCommand(del_cmd);
    
    // Load
    EXPECT_TRUE(rdb.load(handler.getDict(), test_filename));
    
    auto get_cmd = std::make_shared<RESPArray>();
    get_cmd->elements = { RESPBulkString("GET"), RESPBulkString("rdb_bg") };
    auto res = handler.handleCommand(get_cmd);
    EXPECT_EQ(std::get<RESPBulkString>(res), "bg_val");
}
