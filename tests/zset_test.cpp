#include <gtest/gtest.h>
#include "../src/server/command_handler.h"

using namespace inferno::protocol;
using namespace inferno::server;

class ZSetTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear dict implicitly by using unique keys per test
    }
};

TEST_F(ZSetTest, ZAddAndZScore) {
    auto& handler = CommandHandler::instance();
    std::string key = "myzset1";
    
    // ZADD myzset1 10.5 a 5.2 b
    auto zadd_cmd = std::make_shared<RESPArray>();
    zadd_cmd->elements = { RESPBulkString("ZADD"), RESPBulkString(key), 
                           RESPBulkString("10.5"), RESPBulkString("a"), 
                           RESPBulkString("5.2"), RESPBulkString("b") };
    auto res1 = handler.handleCommand(zadd_cmd);
    EXPECT_TRUE(std::holds_alternative<RESPInteger>(res1));
    EXPECT_EQ(std::get<RESPInteger>(res1), 2);
    
    // ZSCORE myzset1 a
    auto zscore_a = std::make_shared<RESPArray>();
    zscore_a->elements = { RESPBulkString("ZSCORE"), RESPBulkString(key), RESPBulkString("a") };
    auto res_a = handler.handleCommand(zscore_a);
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(res_a));
    
    // Exact float formatting might differ slightly, but should match closely
    std::string score_a_str = std::get<RESPBulkString>(res_a);
    EXPECT_EQ(std::stod(score_a_str), 10.5);
    
    // ZSCORE myzset1 not_exist
    auto zscore_missing = std::make_shared<RESPArray>();
    zscore_missing->elements = { RESPBulkString("ZSCORE"), RESPBulkString(key), RESPBulkString("not_exist") };
    auto res_missing = handler.handleCommand(zscore_missing);
    EXPECT_TRUE(std::holds_alternative<RESPNull>(res_missing));
}

TEST_F(ZSetTest, ZRangeOrder) {
    auto& handler = CommandHandler::instance();
    std::string key = "myzset_range";
    
    // ZADD out of order
    auto zadd_cmd = std::make_shared<RESPArray>();
    zadd_cmd->elements = { RESPBulkString("ZADD"), RESPBulkString(key), 
                           RESPBulkString("100"), RESPBulkString("c"), 
                           RESPBulkString("10"), RESPBulkString("a"), 
                           RESPBulkString("50"), RESPBulkString("b") };
    handler.handleCommand(zadd_cmd);
    
    // ZRANGE myzset_range 0 -1
    auto zrange_cmd = std::make_shared<RESPArray>();
    zrange_cmd->elements = { RESPBulkString("ZRANGE"), RESPBulkString(key), 
                             RESPBulkString("0"), RESPBulkString("-1") };
                             
    auto res = handler.handleCommand(zrange_cmd);
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(res));
    auto arr = std::get<std::shared_ptr<RESPArray>>(res);
    
    EXPECT_EQ(arr->elements.size(), 3);
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[0]), "a"); // score 10
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[1]), "b"); // score 50
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[2]), "c"); // score 100
}

TEST_F(ZSetTest, ZRangeByScore) {
    auto& handler = CommandHandler::instance();
    std::string key = "myzset_rangebyscore";
    
    auto zadd_cmd = std::make_shared<RESPArray>();
    zadd_cmd->elements = { RESPBulkString("ZADD"), RESPBulkString(key), 
                           RESPBulkString("1"), RESPBulkString("one"), 
                           RESPBulkString("2"), RESPBulkString("two"), 
                           RESPBulkString("3"), RESPBulkString("three") };
    handler.handleCommand(zadd_cmd);
    
    // ZRANGEBYSCORE key 1.5 3.0
    auto range_cmd = std::make_shared<RESPArray>();
    range_cmd->elements = { RESPBulkString("ZRANGEBYSCORE"), RESPBulkString(key), 
                            RESPBulkString("1.5"), RESPBulkString("3.0") };
                            
    auto res = handler.handleCommand(range_cmd);
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(res));
    auto arr = std::get<std::shared_ptr<RESPArray>>(res);
    
    EXPECT_EQ(arr->elements.size(), 2);
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[0]), "two");
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[1]), "three");
    
    // Test infinities
    auto range_inf = std::make_shared<RESPArray>();
    range_inf->elements = { RESPBulkString("ZRANGEBYSCORE"), RESPBulkString(key), 
                            RESPBulkString("-inf"), RESPBulkString("+inf") };
    auto res_inf = handler.handleCommand(range_inf);
    auto arr_inf = std::get<std::shared_ptr<RESPArray>>(res_inf);
    EXPECT_EQ(arr_inf->elements.size(), 3);
}

TEST_F(ZSetTest, LexicographicalOrdering) {
    auto& handler = CommandHandler::instance();
    std::string key = "myzset_lex";
    
    // ZADD elements with same score
    auto zadd_cmd = std::make_shared<RESPArray>();
    zadd_cmd->elements = { RESPBulkString("ZADD"), RESPBulkString(key), 
                           RESPBulkString("10"), RESPBulkString("banana"), 
                           RESPBulkString("10"), RESPBulkString("apple"), 
                           RESPBulkString("10"), RESPBulkString("cherry") };
    handler.handleCommand(zadd_cmd);
    
    auto zrange_cmd = std::make_shared<RESPArray>();
    zrange_cmd->elements = { RESPBulkString("ZRANGE"), RESPBulkString(key), 
                             RESPBulkString("0"), RESPBulkString("-1") };
                             
    auto res = handler.handleCommand(zrange_cmd);
    auto arr = std::get<std::shared_ptr<RESPArray>>(res);
    
    EXPECT_EQ(arr->elements.size(), 3);
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[0]), "apple");
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[1]), "banana");
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[2]), "cherry");
}
