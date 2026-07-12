#include <gtest/gtest.h>
#include "../src/memory/allocator.h"
#include "../src/storage/value.h"
#include "../src/server/command_handler.h"

using namespace inferno::memory;
using namespace inferno::storage;
using namespace inferno::protocol;
using namespace inferno::server;

class MemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        SharedObjects::initialize();
    }
};

TEST_F(MemoryTest, AllocatorTracksMemory) {
    size_t initial_mem = Allocator::getUsedMemory();
    
    void* ptr = Allocator::allocate(1024);
    EXPECT_GE(Allocator::getUsedMemory(), initial_mem + 1024);
    
    Allocator::deallocate(ptr);
    EXPECT_EQ(Allocator::getUsedMemory(), initial_mem);
}

TEST_F(MemoryTest, SharedIntegersDontAllocate) {
    size_t initial_mem = Allocator::getUsedMemory();
    
    // Create multiple references to the same shared integer (100)
    std::vector<InfernoObject::SharedPtr> objects;
    for (int i = 0; i < 1000; ++i) {
        objects.push_back(InfernoObject::create(100));
    }
    
    // Because 100 is in the shared pool (0-9999), no new memory should have been allocated
    // by Allocator::allocate (only the vector's internal buffer, which uses standard allocator)
    EXPECT_EQ(Allocator::getUsedMemory(), initial_mem);
    
    // All pointers should point to the exact same object
    for (int i = 1; i < 1000; ++i) {
        EXPECT_EQ(objects[0].get(), objects[i].get());
    }
}

TEST_F(MemoryTest, UnsharedIntegersAllocate) {
    size_t initial_mem = Allocator::getUsedMemory();
    
    auto obj = InfernoObject::create(15000); // Outside shared pool
    
    // Should have allocated memory for the InfernoObject
    EXPECT_GT(Allocator::getUsedMemory(), initial_mem);
    
    // When obj goes out of scope, it should free the memory
    obj.reset();
    
    EXPECT_EQ(Allocator::getUsedMemory(), initial_mem);
}

TEST_F(MemoryTest, MemoryStatsCommand) {
    auto& handler = CommandHandler::instance();
    
    auto mem_cmd = std::make_shared<RESPArray>();
    mem_cmd->elements = { RESPBulkString("MEMORY"), RESPBulkString("STATS") };
    
    auto res = handler.handleCommand(mem_cmd);
    EXPECT_TRUE(std::holds_alternative<std::shared_ptr<RESPArray>>(res));
    
    auto arr = std::get<std::shared_ptr<RESPArray>>(res);
    EXPECT_GE(arr->elements.size(), 6); // peak, total, dict size
}
