#include <gtest/gtest.h>
#include "../src/server/command_handler.h"
#include <memory>
#include <string>

using namespace inferno::protocol;
using namespace inferno::server;

class WatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& handler = CommandHandler::instance();
        auto cmd = std::make_shared<RESPArray>();
        cmd->elements = { RESPBulkString("DEL"), RESPBulkString("w_key") };
        handler.handleCommand(cmd, 1);
        handler.handleClientDisconnect(1);
    }
    
    void TearDown() override {
        CommandHandler::instance().handleClientDisconnect(1);
    }
};

TEST_F(WatchTest, WatchSuccess) {
    auto& handler = CommandHandler::instance();
    network::socket_t fd = 1;
    
    // 1. WATCH w_key
    auto watch_cmd = std::make_shared<RESPArray>();
    watch_cmd->elements = { RESPBulkString("WATCH"), RESPBulkString("w_key") };
    auto res_watch = handler.handleCommand(watch_cmd, fd);
    EXPECT_TRUE(res_watch.has_value());
    
    // 2. MULTI
    auto multi = std::make_shared<RESPArray>();
    multi->elements = { RESPBulkString("MULTI") };
    handler.handleCommand(multi, fd);
    
    // 3. SET w_key val
    auto set_cmd = std::make_shared<RESPArray>();
    set_cmd->elements = { RESPBulkString("SET"), RESPBulkString("w_key"), RESPBulkString("val") };
    handler.handleCommand(set_cmd, fd);
    
    // 4. EXEC (Should succeed since nobody else touched w_key)
    auto exec = std::make_shared<RESPArray>();
    exec->elements = { RESPBulkString("EXEC") };
    auto res_exec = handler.handleCommand(exec, fd);
    
    ASSERT_TRUE(res_exec.has_value());
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(*res_exec));
    auto results = std::get<std::shared_ptr<RESPArray>>(*res_exec);
    ASSERT_EQ(results->elements.size(), 1); // The SET should have succeeded
}

TEST_F(WatchTest, WatchAbort) {
    auto& handler = CommandHandler::instance();
    network::socket_t fd1 = 1;
    network::socket_t fd2 = 2; // Different client
    
    // 1. Client 1 WATCH w_key
    auto watch_cmd = std::make_shared<RESPArray>();
    watch_cmd->elements = { RESPBulkString("WATCH"), RESPBulkString("w_key") };
    handler.handleCommand(watch_cmd, fd1);
    
    // 2. Client 1 MULTI
    auto multi = std::make_shared<RESPArray>();
    multi->elements = { RESPBulkString("MULTI") };
    handler.handleCommand(multi, fd1);
    
    // 3. Client 1 queues SET w_key val1
    auto set_cmd1 = std::make_shared<RESPArray>();
    set_cmd1->elements = { RESPBulkString("SET"), RESPBulkString("w_key"), RESPBulkString("val1") };
    handler.handleCommand(set_cmd1, fd1);
    
    // 4. Client 2 mutates w_key! (SET w_key val2)
    auto set_cmd2 = std::make_shared<RESPArray>();
    set_cmd2->elements = { RESPBulkString("SET"), RESPBulkString("w_key"), RESPBulkString("val2") };
    handler.handleCommand(set_cmd2, fd2);
    
    // 5. Client 1 EXEC (Should ABORT and return null array)
    auto exec = std::make_shared<RESPArray>();
    exec->elements = { RESPBulkString("EXEC") };
    auto res_exec = handler.handleCommand(exec, fd1);
    
    ASSERT_TRUE(res_exec.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPNull>(*res_exec)); // The transaction was aborted!
    
    // 6. Verify w_key is val2
    auto get_cmd = std::make_shared<RESPArray>();
    get_cmd->elements = { RESPBulkString("GET"), RESPBulkString("w_key") };
    auto res_get = handler.handleCommand(get_cmd, fd1);
    
    ASSERT_TRUE(res_get.has_value());
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(*res_get));
    EXPECT_EQ(std::get<RESPBulkString>(*res_get).value, "val2");
}
