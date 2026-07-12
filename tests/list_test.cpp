#include <gtest/gtest.h>
#include "../src/server/command_handler.h"

using namespace inferno::protocol;
using namespace inferno::server;

class ListTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear the dictionary before each test
        auto& dict = CommandHandler::instance().getDict();
        // Since it's a singleton, we need to manually clean up to prevent test pollution
        // Actually for a simple test we can just use unique keys
    }
};

TEST_F(ListTest, LPushAndLLen) {
    auto& handler = CommandHandler::instance();
    std::string key = "mylist_push_len";
    
    auto lpush_cmd = std::make_shared<RESPArray>();
    lpush_cmd->elements = { RESPBulkString("LPUSH"), RESPBulkString(key), RESPBulkString("world") };
    auto res1 = handler.handleCommand(lpush_cmd);
    EXPECT_TRUE(std::holds_alternative<RESPInteger>(res1));
    EXPECT_EQ(std::get<RESPInteger>(res1), 1);
    
    auto lpush_cmd2 = std::make_shared<RESPArray>();
    lpush_cmd2->elements = { RESPBulkString("LPUSH"), RESPBulkString(key), RESPBulkString("hello") };
    auto res2 = handler.handleCommand(lpush_cmd2);
    EXPECT_EQ(std::get<RESPInteger>(res2), 2);
    
    auto llen_cmd = std::make_shared<RESPArray>();
    llen_cmd->elements = { RESPBulkString("LLEN"), RESPBulkString(key) };
    auto res3 = handler.handleCommand(llen_cmd);
    EXPECT_EQ(std::get<RESPInteger>(res3), 2);
}

TEST_F(ListTest, LPopAndRPop) {
    auto& handler = CommandHandler::instance();
    std::string key = "mylist_pop";
    
    auto rpush_cmd = std::make_shared<RESPArray>();
    // RPUSH mylist_pop one two three
    rpush_cmd->elements = { RESPBulkString("RPUSH"), RESPBulkString(key), 
                            RESPBulkString("one"), RESPBulkString("two"), RESPBulkString("three") };
    handler.handleCommand(rpush_cmd);
    
    auto lpop_cmd = std::make_shared<RESPArray>();
    lpop_cmd->elements = { RESPBulkString("LPOP"), RESPBulkString(key) };
    auto res1 = handler.handleCommand(lpop_cmd);
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(res1));
    EXPECT_EQ(std::get<RESPBulkString>(res1), "one");
    
    auto rpop_cmd = std::make_shared<RESPArray>();
    rpop_cmd->elements = { RESPBulkString("RPOP"), RESPBulkString(key) };
    auto res2 = handler.handleCommand(rpop_cmd);
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(res2));
    EXPECT_EQ(std::get<RESPBulkString>(res2), "three");
    
    auto llen_cmd = std::make_shared<RESPArray>();
    llen_cmd->elements = { RESPBulkString("LLEN"), RESPBulkString(key) };
    auto res3 = handler.handleCommand(llen_cmd);
    EXPECT_EQ(std::get<RESPInteger>(res3), 1); // Only "two" remains
}

TEST_F(ListTest, LRange) {
    auto& handler = CommandHandler::instance();
    std::string key = "mylist_range";
    
    auto rpush_cmd = std::make_shared<RESPArray>();
    rpush_cmd->elements = { RESPBulkString("RPUSH"), RESPBulkString(key), 
                            RESPBulkString("0"), RESPBulkString("1"), RESPBulkString("2"), RESPBulkString("3") };
    handler.handleCommand(rpush_cmd);
    
    auto lrange_cmd = std::make_shared<RESPArray>();
    // LRANGE mylist_range 1 2
    lrange_cmd->elements = { RESPBulkString("LRANGE"), RESPBulkString(key), 
                             RESPBulkString("1"), RESPBulkString("2") };
    
    auto res = handler.handleCommand(lrange_cmd);
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(res));
    
    auto arr = std::get<std::shared_ptr<RESPArray>>(res);
    EXPECT_EQ(arr->elements.size(), 2);
    
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[0]), "1");
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[1]), "2");
}

TEST_F(ListTest, WrongTypeOperations) {
    auto& handler = CommandHandler::instance();
    std::string key = "mylist_wrongtype";
    
    // Set as string
    auto set_cmd = std::make_shared<RESPArray>();
    set_cmd->elements = { RESPBulkString("SET"), RESPBulkString(key), RESPBulkString("string_value") };
    handler.handleCommand(set_cmd);
    
    // Try LPUSH on string
    auto lpush_cmd = std::make_shared<RESPArray>();
    lpush_cmd->elements = { RESPBulkString("LPUSH"), RESPBulkString(key), RESPBulkString("val") };
    auto res = handler.handleCommand(lpush_cmd);
    
    EXPECT_TRUE(std::holds_alternative<RESPError>(res));
    std::string err_msg = std::get<RESPError>(res).message;
    EXPECT_NE(err_msg.find("WRONGTYPE"), std::string::npos);
}
