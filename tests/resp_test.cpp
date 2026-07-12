#include <gtest/gtest.h>
#include "../src/protocol/resp_parser.h"

using namespace inferno::protocol;

TEST(RESPParserTest, ParseSimpleString) {
    RESPParser parser;
    parser.feed("+OK\r\n", 5);
    RESPObject obj;
    EXPECT_EQ(parser.parse(obj), RESPParser::ParseResult::OK);
    EXPECT_TRUE(std::holds_alternative<RESPSimpleString>(obj));
    EXPECT_EQ(std::get<RESPSimpleString>(obj), "OK");
}

TEST(RESPParserTest, ParseError) {
    RESPParser parser;
    parser.feed("-Error message\r\n", 16);
    RESPObject obj;
    EXPECT_EQ(parser.parse(obj), RESPParser::ParseResult::OK);
    EXPECT_TRUE(std::holds_alternative<RESPError>(obj));
    EXPECT_EQ(std::get<RESPError>(obj), "Error message");
}

TEST(RESPParserTest, ParseInteger) {
    RESPParser parser;
    parser.feed(":1000\r\n", 7);
    RESPObject obj;
    EXPECT_EQ(parser.parse(obj), RESPParser::ParseResult::OK);
    EXPECT_TRUE(std::holds_alternative<RESPInteger>(obj));
    EXPECT_EQ(std::get<RESPInteger>(obj), 1000);
}

TEST(RESPParserTest, ParseBulkString) {
    RESPParser parser;
    std::string data = "$5\r\nhello\r\n";
    parser.feed(data.c_str(), data.length());
    RESPObject obj;
    EXPECT_EQ(parser.parse(obj), RESPParser::ParseResult::OK);
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(obj));
    EXPECT_EQ(std::get<RESPBulkString>(obj), "hello");
}

TEST(RESPParserTest, ParseNullBulkString) {
    RESPParser parser;
    std::string data = "$-1\r\n";
    parser.feed(data.c_str(), data.length());
    RESPObject obj;
    EXPECT_EQ(parser.parse(obj), RESPParser::ParseResult::OK);
    EXPECT_TRUE(std::holds_alternative<RESPNull>(obj));
}

TEST(RESPParserTest, ParseArray) {
    RESPParser parser;
    // *2\r\n$4\r\nECHO\r\n$11\r\nhello world\r\n
    std::string data = "*2\r\n$4\r\nECHO\r\n$11\r\nhello world\r\n";
    parser.feed(data.c_str(), data.length());
    RESPObject obj;
    EXPECT_EQ(parser.parse(obj), RESPParser::ParseResult::OK);
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(obj));
    auto arr = std::get<std::shared_ptr<RESPArray>>(obj);
    EXPECT_EQ(arr->elements.size(), 2);
    
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(arr->elements[0]));
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[0]), "ECHO");
    
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(arr->elements[1]));
    EXPECT_EQ(std::get<RESPBulkString>(arr->elements[1]), "hello world");
}

TEST(RESPParserTest, IncrementalParsing) {
    RESPParser parser;
    std::string data = "*2\r\n$4\r\nECHO\r\n$11\r\nhello world\r\n";
    
    RESPObject obj;
    // Feed one byte at a time
    for (size_t i = 0; i < data.length() - 1; ++i) {
        parser.feed(&data[i], 1);
        EXPECT_EQ(parser.parse(obj), RESPParser::ParseResult::NEED_MORE);
    }
    
    // Feed the last byte
    parser.feed(&data[data.length() - 1], 1);
    EXPECT_EQ(parser.parse(obj), RESPParser::ParseResult::OK);
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(obj));
    auto arr = std::get<std::shared_ptr<RESPArray>>(obj);
    EXPECT_EQ(arr->elements.size(), 2);
}
