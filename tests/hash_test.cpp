#include <gtest/gtest.h>
#include "../src/server/command_handler.h"
#include <memory>
#include <string>

using namespace inferno::protocol;
using namespace inferno::server;

class HashTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& handler = CommandHandler::instance();
        auto cmd = std::make_shared<RESPArray>();
        cmd->elements = { RESPBulkString("DEL"), RESPBulkString("myhash") };
        handler.handleCommand(cmd, 1);
    }
};

TEST_F(HashTest, HsetAndHget) {
    auto& handler = CommandHandler::instance();
    network::socket_t fd = 1;
    
    // 1. HSET myhash field1 val1 field2 val2
    auto hset = std::make_shared<RESPArray>();
    hset->elements = { RESPBulkString("HSET"), RESPBulkString("myhash"), 
                       RESPBulkString("field1"), RESPBulkString("val1"),
                       RESPBulkString("field2"), RESPBulkString("val2") };
    auto res_hset = handler.handleCommand(hset, fd);
    ASSERT_TRUE(res_hset.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPInteger>(*res_hset));
    EXPECT_EQ(std::get<RESPInteger>(*res_hset).value, 2);
    
    // 2. HSET myhash field1 val3 (Updating an existing field)
    auto hset_update = std::make_shared<RESPArray>();
    hset_update->elements = { RESPBulkString("HSET"), RESPBulkString("myhash"), 
                              RESPBulkString("field1"), RESPBulkString("val3") };
    auto res_hset_update = handler.handleCommand(hset_update, fd);
    // std::unordered_map::insert_or_assign returns false for insertion when it updates
    EXPECT_EQ(std::get<RESPInteger>(*res_hset_update).value, 0);
    
    // 3. HGET myhash field1 (Should be val3)
    auto hget_1 = std::make_shared<RESPArray>();
    hget_1->elements = { RESPBulkString("HGET"), RESPBulkString("myhash"), RESPBulkString("field1") };
    auto res_hget_1 = handler.handleCommand(hget_1, fd);
    ASSERT_TRUE(res_hget_1.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(*res_hget_1));
    EXPECT_EQ(std::get<RESPBulkString>(*res_hget_1).value, "val3");
    
    // 4. HGET myhash nofield (Should be null)
    auto hget_2 = std::make_shared<RESPArray>();
    hget_2->elements = { RESPBulkString("HGET"), RESPBulkString("myhash"), RESPBulkString("nofield") };
    auto res_hget_2 = handler.handleCommand(hget_2, fd);
    ASSERT_TRUE(res_hget_2.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPNull>(*res_hget_2));
}

TEST_F(HashTest, HdelAndHgetall) {
    auto& handler = CommandHandler::instance();
    network::socket_t fd = 1;
    
    // HSET myhash f1 v1 f2 v2 f3 v3
    auto hset = std::make_shared<RESPArray>();
    hset->elements = { RESPBulkString("HSET"), RESPBulkString("myhash"), 
                       RESPBulkString("f1"), RESPBulkString("v1"),
                       RESPBulkString("f2"), RESPBulkString("v2"),
                       RESPBulkString("f3"), RESPBulkString("v3") };
    handler.handleCommand(hset, fd);
    
    // HDEL myhash f1 f3
    auto hdel = std::make_shared<RESPArray>();
    hdel->elements = { RESPBulkString("HDEL"), RESPBulkString("myhash"), RESPBulkString("f1"), RESPBulkString("f3") };
    auto res_hdel = handler.handleCommand(hdel, fd);
    EXPECT_EQ(std::get<RESPInteger>(*res_hdel).value, 2);
    
    // HGETALL myhash
    auto hgetall = std::make_shared<RESPArray>();
    hgetall->elements = { RESPBulkString("HGETALL"), RESPBulkString("myhash") };
    auto res_hgetall = handler.handleCommand(hgetall, fd);
    
    ASSERT_TRUE(res_hgetall.has_value());
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(*res_hgetall));
    auto arr = std::get<std::shared_ptr<RESPArray>>(*res_hgetall);
    
    // Should have 2 elements: "f2" and "v2"
    ASSERT_EQ(arr->elements.size(), 2);
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(arr->elements[0]));
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(arr->elements[1]));
    
    std::string k = std::get<RESPBulkString>(arr->elements[0]).value;
    std::string v = std::get<RESPBulkString>(arr->elements[1]).value;
    EXPECT_EQ(k, "f2");
    EXPECT_EQ(v, "v2");
    
    // HDEL myhash f2 (Should empty the hash and delete the key)
    auto hdel2 = std::make_shared<RESPArray>();
    hdel2->elements = { RESPBulkString("HDEL"), RESPBulkString("myhash"), RESPBulkString("f2") };
    handler.handleCommand(hdel2, fd);
    
    // Check key is gone
    auto exists = std::make_shared<RESPArray>();
    exists->elements = { RESPBulkString("EXISTS"), RESPBulkString("myhash") };
    auto res_exists = handler.handleCommand(exists, fd);
    EXPECT_EQ(std::get<RESPInteger>(*res_exists).value, 0);
}
