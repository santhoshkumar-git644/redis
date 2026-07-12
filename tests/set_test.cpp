#include <gtest/gtest.h>
#include "../src/server/command_handler.h"
#include <memory>
#include <string>

using namespace inferno::protocol;
using namespace inferno::server;

class SetTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& handler = CommandHandler::instance();
        auto cmd = std::make_shared<RESPArray>();
        cmd->elements = { RESPBulkString("DEL"), RESPBulkString("myset") };
        handler.handleCommand(cmd, 1);
    }
};

TEST_F(SetTest, SaddAndSmembers) {
    auto& handler = CommandHandler::instance();
    network::socket_t fd = 1;
    
    // 1. SADD myset a b c
    auto sadd = std::make_shared<RESPArray>();
    sadd->elements = { RESPBulkString("SADD"), RESPBulkString("myset"), 
                       RESPBulkString("a"), RESPBulkString("b"), RESPBulkString("c") };
    auto res_sadd = handler.handleCommand(sadd, fd);
    ASSERT_TRUE(res_sadd.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPInteger>(*res_sadd));
    EXPECT_EQ(std::get<RESPInteger>(*res_sadd).value, 3);
    
    // 2. SADD myset a d (a is duplicate, only d added)
    auto sadd2 = std::make_shared<RESPArray>();
    sadd2->elements = { RESPBulkString("SADD"), RESPBulkString("myset"), 
                        RESPBulkString("a"), RESPBulkString("d") };
    auto res_sadd2 = handler.handleCommand(sadd2, fd);
    EXPECT_EQ(std::get<RESPInteger>(*res_sadd2).value, 1);
    
    // 3. SMEMBERS
    auto smembers = std::make_shared<RESPArray>();
    smembers->elements = { RESPBulkString("SMEMBERS"), RESPBulkString("myset") };
    auto res_smembers = handler.handleCommand(smembers, fd);
    
    ASSERT_TRUE(res_smembers.has_value());
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(*res_smembers));
    auto arr = std::get<std::shared_ptr<RESPArray>>(*res_smembers);
    EXPECT_EQ(arr->elements.size(), 4); // a, b, c, d
}

TEST_F(SetTest, SremAndSismember) {
    auto& handler = CommandHandler::instance();
    network::socket_t fd = 1;
    
    // SADD myset x y
    auto sadd = std::make_shared<RESPArray>();
    sadd->elements = { RESPBulkString("SADD"), RESPBulkString("myset"), RESPBulkString("x"), RESPBulkString("y") };
    handler.handleCommand(sadd, fd);
    
    // SISMEMBER myset x (should be 1)
    auto sismember_x = std::make_shared<RESPArray>();
    sismember_x->elements = { RESPBulkString("SISMEMBER"), RESPBulkString("myset"), RESPBulkString("x") };
    auto res_sis_x = handler.handleCommand(sismember_x, fd);
    EXPECT_EQ(std::get<RESPInteger>(*res_sis_x).value, 1);
    
    // SISMEMBER myset z (should be 0)
    auto sismember_z = std::make_shared<RESPArray>();
    sismember_z->elements = { RESPBulkString("SISMEMBER"), RESPBulkString("myset"), RESPBulkString("z") };
    auto res_sis_z = handler.handleCommand(sismember_z, fd);
    EXPECT_EQ(std::get<RESPInteger>(*res_sis_z).value, 0);
    
    // SREM myset x
    auto srem = std::make_shared<RESPArray>();
    srem->elements = { RESPBulkString("SREM"), RESPBulkString("myset"), RESPBulkString("x") };
    auto res_srem = handler.handleCommand(srem, fd);
    EXPECT_EQ(std::get<RESPInteger>(*res_srem).value, 1);
    
    // SISMEMBER myset x (now 0)
    res_sis_x = handler.handleCommand(sismember_x, fd);
    EXPECT_EQ(std::get<RESPInteger>(*res_sis_x).value, 0);
}
