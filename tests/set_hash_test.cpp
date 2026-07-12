#include <gtest/gtest.h>
#include "../src/server/command_handler.h"

using namespace inferno::protocol;
using namespace inferno::server;

class SetHashTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear dict implicitly by using unique keys
    }
};

TEST_F(SetHashTest, SetOperations) {
    auto& handler = CommandHandler::instance();
    std::string key = "myset";
    
    // SADD myset a b c a
    auto sadd_cmd = std::make_shared<RESPArray>();
    sadd_cmd->elements = { RESPBulkString("SADD"), RESPBulkString(key), 
                           RESPBulkString("a"), RESPBulkString("b"), RESPBulkString("c"), RESPBulkString("a") };
    auto res1 = handler.handleCommand(sadd_cmd);
    EXPECT_TRUE(std::holds_alternative<RESPInteger>(res1));
    EXPECT_EQ(std::get<RESPInteger>(res1), 3); // "a" is added only once
    
    // SISMEMBER myset a
    auto sismember_cmd = std::make_shared<RESPArray>();
    sismember_cmd->elements = { RESPBulkString("SISMEMBER"), RESPBulkString(key), RESPBulkString("a") };
    auto res2 = handler.handleCommand(sismember_cmd);
    EXPECT_EQ(std::get<RESPInteger>(res2), 1);
    
    // SISMEMBER myset z
    auto sismember_z_cmd = std::make_shared<RESPArray>();
    sismember_z_cmd->elements = { RESPBulkString("SISMEMBER"), RESPBulkString(key), RESPBulkString("z") };
    auto res3 = handler.handleCommand(sismember_z_cmd);
    EXPECT_EQ(std::get<RESPInteger>(res3), 0);
    
    // SREM myset a b
    auto srem_cmd = std::make_shared<RESPArray>();
    srem_cmd->elements = { RESPBulkString("SREM"), RESPBulkString(key), RESPBulkString("a"), RESPBulkString("b") };
    auto res4 = handler.handleCommand(srem_cmd);
    EXPECT_EQ(std::get<RESPInteger>(res4), 2);
    
    // SMEMBERS myset
    auto smembers_cmd = std::make_shared<RESPArray>();
    smembers_cmd->elements = { RESPBulkString("SMEMBERS"), RESPBulkString(key) };
    auto res5 = handler.handleCommand(smembers_cmd);
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(res5));
    auto arr = std::get<std::shared_ptr<RESPArray>>(res5);
    EXPECT_EQ(arr->elements.size(), 1);
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[0]), "c");
}

TEST_F(SetHashTest, HashOperations) {
    auto& handler = CommandHandler::instance();
    std::string key = "myhash";
    
    // HSET myhash field1 val1 field2 val2
    auto hset_cmd = std::make_shared<RESPArray>();
    hset_cmd->elements = { RESPBulkString("HSET"), RESPBulkString(key), 
                           RESPBulkString("field1"), RESPBulkString("val1"),
                           RESPBulkString("field2"), RESPBulkString("val2") };
    auto res1 = handler.handleCommand(hset_cmd);
    EXPECT_TRUE(std::holds_alternative<RESPInteger>(res1));
    EXPECT_EQ(std::get<RESPInteger>(res1), 2);
    
    // HGET myhash field1
    auto hget_cmd = std::make_shared<RESPArray>();
    hget_cmd->elements = { RESPBulkString("HGET"), RESPBulkString(key), RESPBulkString("field1") };
    auto res2 = handler.handleCommand(hget_cmd);
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(res2));
    EXPECT_EQ(std::get<RESPBulkString>(res2), "val1");
    
    // HGET myhash field3 (not found)
    auto hget_cmd2 = std::make_shared<RESPArray>();
    hget_cmd2->elements = { RESPBulkString("HGET"), RESPBulkString(key), RESPBulkString("field3") };
    auto res3 = handler.handleCommand(hget_cmd2);
    EXPECT_TRUE(std::holds_alternative<RESPNull>(res3));
    
    // HDEL myhash field1
    auto hdel_cmd = std::make_shared<RESPArray>();
    hdel_cmd->elements = { RESPBulkString("HDEL"), RESPBulkString(key), RESPBulkString("field1") };
    auto res4 = handler.handleCommand(hdel_cmd);
    EXPECT_EQ(std::get<RESPInteger>(res4), 1);
    
    // HGETALL myhash
    auto hgetall_cmd = std::make_shared<RESPArray>();
    hgetall_cmd->elements = { RESPBulkString("HGETALL"), RESPBulkString(key) };
    auto res5 = handler.handleCommand(hgetall_cmd);
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(res5));
    auto arr = std::get<std::shared_ptr<RESPArray>>(res5);
    EXPECT_EQ(arr->elements.size(), 2); // field2 and val2
    // order in unordered_map is not guaranteed, but there's only 1 pair
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[0]), "field2");
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[1]), "val2");
}

TEST_F(SetHashTest, WrongTypeOperations) {
    auto& handler = CommandHandler::instance();
    std::string key = "wrongtype_set";
    
    // Set as string
    auto set_cmd = std::make_shared<RESPArray>();
    set_cmd->elements = { RESPBulkString("SET"), RESPBulkString(key), RESPBulkString("string_value") };
    handler.handleCommand(set_cmd);
    
    // Try SADD on string
    auto sadd_cmd = std::make_shared<RESPArray>();
    sadd_cmd->elements = { RESPBulkString("SADD"), RESPBulkString(key), RESPBulkString("val") };
    auto res = handler.handleCommand(sadd_cmd);
    
    EXPECT_TRUE(std::holds_alternative<RESPError>(res));
}
