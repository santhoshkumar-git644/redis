#include <gtest/gtest.h>
#include "../src/server/command_handler.h"
#include <memory>
#include <string>

using namespace inferno::protocol;
using namespace inferno::server;

class TransactionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear global dict for a fresh start
        auto& handler = CommandHandler::instance();
        auto cmd = std::make_shared<RESPArray>();
        cmd->elements = { RESPBulkString("DEL"), RESPBulkString("tx_key") };
        handler.handleCommand(cmd, 1);
        handler.handleClientDisconnect(1);
    }
    
    void TearDown() override {
        CommandHandler::instance().handleClientDisconnect(1);
    }
};

TEST_F(TransactionTest, MultiExecQueue) {
    auto& handler = CommandHandler::instance();
    network::socket_t fd = 1; // dummy fd
    
    // 1. Send MULTI
    auto multi = std::make_shared<RESPArray>();
    multi->elements = { RESPBulkString("MULTI") };
    auto res_multi = handler.handleCommand(multi, fd);
    ASSERT_TRUE(res_multi.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPSimpleString>(*res_multi));
    EXPECT_EQ(std::get<RESPSimpleString>(*res_multi).value, "OK");
    
    // 2. Queue SET tx_key tx_val
    auto set_cmd = std::make_shared<RESPArray>();
    set_cmd->elements = { RESPBulkString("SET"), RESPBulkString("tx_key"), RESPBulkString("tx_val") };
    auto res_set = handler.handleCommand(set_cmd, fd);
    ASSERT_TRUE(res_set.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPSimpleString>(*res_set));
    EXPECT_EQ(std::get<RESPSimpleString>(*res_set).value, "QUEUED");
    
    // Verify it's not set yet
    auto get_cmd = std::make_shared<RESPArray>();
    get_cmd->elements = { RESPBulkString("GET"), RESPBulkString("tx_key") };
    auto res_get_out = handler.handleCommand(get_cmd, 2); // Different client
    ASSERT_TRUE(res_get_out.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPNull>(*res_get_out)); // Should be null
    
    // 3. Queue GET tx_key
    auto res_get_in = handler.handleCommand(get_cmd, fd);
    ASSERT_TRUE(res_get_in.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPSimpleString>(*res_get_in));
    EXPECT_EQ(std::get<RESPSimpleString>(*res_get_in).value, "QUEUED");
    
    // 4. EXEC
    auto exec_cmd = std::make_shared<RESPArray>();
    exec_cmd->elements = { RESPBulkString("EXEC") };
    auto res_exec = handler.handleCommand(exec_cmd, fd);
    ASSERT_TRUE(res_exec.has_value());
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(*res_exec));
    
    auto results = std::get<std::shared_ptr<RESPArray>>(*res_exec);
    ASSERT_EQ(results->elements.size(), 2);
    
    // Check first result (OK from SET)
    EXPECT_TRUE(std::holds_alternative<RESPSimpleString>(results->elements[0]));
    EXPECT_EQ(std::get<RESPSimpleString>(results->elements[0]).value, "OK");
    
    // Check second result (tx_val from GET)
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(results->elements[1]));
    EXPECT_EQ(std::get<RESPBulkString>(results->elements[1]).value, "tx_val");
}

TEST_F(TransactionTest, Discard) {
    auto& handler = CommandHandler::instance();
    network::socket_t fd = 1;
    
    // MULTI
    auto multi = std::make_shared<RESPArray>();
    multi->elements = { RESPBulkString("MULTI") };
    handler.handleCommand(multi, fd);
    
    // SET
    auto set_cmd = std::make_shared<RESPArray>();
    set_cmd->elements = { RESPBulkString("SET"), RESPBulkString("tx_key"), RESPBulkString("discard_val") };
    handler.handleCommand(set_cmd, fd);
    
    // DISCARD
    auto discard = std::make_shared<RESPArray>();
    discard->elements = { RESPBulkString("DISCARD") };
    auto res_discard = handler.handleCommand(discard, fd);
    ASSERT_TRUE(res_discard.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPSimpleString>(*res_discard));
    EXPECT_EQ(std::get<RESPSimpleString>(*res_discard).value, "OK");
    
    // Verify SET did not execute
    auto get_cmd = std::make_shared<RESPArray>();
    get_cmd->elements = { RESPBulkString("GET"), RESPBulkString("tx_key") };
    auto res_get = handler.handleCommand(get_cmd, fd);
    ASSERT_TRUE(res_get.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPNull>(*res_get));
}

TEST_F(TransactionTest, Errors) {
    auto& handler = CommandHandler::instance();
    network::socket_t fd = 1;
    
    // EXEC without MULTI
    auto exec = std::make_shared<RESPArray>();
    exec->elements = { RESPBulkString("EXEC") };
    auto err1 = handler.handleCommand(exec, fd);
    ASSERT_TRUE(err1.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPError>(*err1));
    
    // DISCARD without MULTI
    auto discard = std::make_shared<RESPArray>();
    discard->elements = { RESPBulkString("DISCARD") };
    auto err2 = handler.handleCommand(discard, fd);
    ASSERT_TRUE(err2.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPError>(*err2));
}
