#include <gtest/gtest.h>
#include "../src/storage/dict.h"
#include "../src/server/command_handler.h"

using namespace inferno::storage;
using namespace inferno::protocol;
using namespace inferno::server;

TEST(DictTest, SetAndGet) {
    Dict dict;
    dict.set("key1", InfernoObject::create("value1"));
    dict.set("key2", InfernoObject::create(100));

    auto val1 = dict.get("key1");
    EXPECT_TRUE(val1 != nullptr);
    EXPECT_EQ(val1->toString(), "value1");

    auto val2 = dict.get("key2");
    EXPECT_TRUE(val2 != nullptr);
    int64_t int_val = 0;
    EXPECT_TRUE(val2->getInt(int_val));
    EXPECT_EQ(int_val, 100);
}

TEST(DictTest, DeleteAndExists) {
    Dict dict;
    dict.set("key1", InfernoObject::create("value1"));
    EXPECT_TRUE(dict.exists("key1"));
    
    EXPECT_TRUE(dict.del("key1"));
    EXPECT_FALSE(dict.exists("key1"));
    EXPECT_FALSE(dict.del("key1")); // Should return false if not found
}

TEST(DictTest, ResizeAndCollisions) {
    Dict dict;
    // Insert many elements to trigger resize
    for (int i = 0; i < 100; ++i) {
        dict.set("key" + std::to_string(i), InfernoObject::create(i));
    }
    
    EXPECT_EQ(dict.size(), 100);
    
    for (int i = 0; i < 100; ++i) {
        auto val = dict.get("key" + std::to_string(i));
        EXPECT_TRUE(val != nullptr);
        int64_t int_val = 0;
        EXPECT_TRUE(val->getInt(int_val));
        EXPECT_EQ(int_val, i);
    }
}

TEST(CommandHandlerTest, BasicCommands) {
    auto& handler = CommandHandler::instance();
    // Clear dict just in case
    // (Since it's a singleton, we need to be careful with state between tests)
    
    // SET test
    auto set_cmd = std::make_shared<RESPArray>();
    set_cmd->elements = { RESPBulkString("SET"), RESPBulkString("mykey"), RESPBulkString("myvalue") };
    auto set_res = handler.handleCommand(set_cmd);
    EXPECT_TRUE(std::holds_alternative<RESPSimpleString>(set_res));
    EXPECT_EQ(std::get<RESPSimpleString>(set_res), "OK");
    
    // GET test
    auto get_cmd = std::make_shared<RESPArray>();
    get_cmd->elements = { RESPBulkString("GET"), RESPBulkString("mykey") };
    auto get_res = handler.handleCommand(get_cmd);
    EXPECT_TRUE(std::holds_alternative<RESPBulkString>(get_res));
    EXPECT_EQ(std::get<RESPBulkString>(get_res), "myvalue");

    // INCR test
    auto set_int_cmd = std::make_shared<RESPArray>();
    set_int_cmd->elements = { RESPBulkString("SET"), RESPBulkString("counter"), RESPBulkString("10") };
    handler.handleCommand(set_int_cmd);
    
    auto incr_cmd = std::make_shared<RESPArray>();
    incr_cmd->elements = { RESPBulkString("INCR"), RESPBulkString("counter") };
    auto incr_res = handler.handleCommand(incr_cmd);
    EXPECT_TRUE(std::holds_alternative<RESPInteger>(incr_res));
    EXPECT_EQ(std::get<RESPInteger>(incr_res), 11);
}
