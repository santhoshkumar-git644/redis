#include <gtest/gtest.h>
#include "../src/protocol/resp_parser.h"
#include <string>
#include <memory>
#include <variant>

using namespace inferno::protocol;

TEST(PipeliningTest, ParseMultipleCommands) {
    RESPParser parser;
    
    // Simulate a client sending 3 PING commands in a single TCP packet
    std::string pipelined_data = 
        "*1\r\n$4\r\nPING\r\n"
        "*1\r\n$4\r\nPING\r\n"
        "*1\r\n$4\r\nPING\r\n";
        
    parser.feed(pipelined_data.c_str(), pipelined_data.length());
    
    RESPObject obj;
    int parsed_count = 0;
    
    while (parser.parse(obj) == RESPParser::ParseResult::OK) {
        ASSERT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(obj));
        auto arr = std::get<std::shared_ptr<RESPArray>>(obj);
        ASSERT_EQ(arr->elements.size(), 1);
        ASSERT_TRUE(std::holds_alternative<RESPBulkString>(arr->elements[0]));
        EXPECT_EQ(std::get<RESPBulkString>(arr->elements[0]).value, "PING");
        parsed_count++;
    }
    
    EXPECT_EQ(parsed_count, 3);
}

TEST(PipeliningTest, ParsePartialCommands) {
    RESPParser parser;
    
    // Send 1.5 commands
    std::string chunk1 = "*1\r\n$4\r\nPING\r\n*1\r\n$4\r\nPI";
    parser.feed(chunk1.c_str(), chunk1.length());
    
    RESPObject obj;
    
    // First command should parse
    ASSERT_EQ(parser.parse(obj), RESPParser::ParseResult::OK);
    
    // Second command is incomplete
    ASSERT_EQ(parser.parse(obj), RESPParser::ParseResult::NEED_MORE);
    
    // Send the rest of the second command
    std::string chunk2 = "NG\r\n";
    parser.feed(chunk2.c_str(), chunk2.length());
    
    // Now it should parse
    ASSERT_EQ(parser.parse(obj), RESPParser::ParseResult::OK);
    
    // No more commands
    ASSERT_EQ(parser.parse(obj), RESPParser::ParseResult::NEED_MORE);
}
