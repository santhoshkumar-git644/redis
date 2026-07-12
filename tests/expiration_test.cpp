#include <gtest/gtest.h>
#include "../src/storage/dict.h"
#include <thread>
#include <chrono>

using namespace inferno::storage;

TEST(ExpirationTest, SetExpireAndLazyDelete) {
    Dict dict;
    dict.set("key1", InfernoObject::create("val1"));
    
    // Set expiry 50ms from now
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    dict.setExpire("key1", now + 50);
    
    EXPECT_TRUE(dict.exists("key1"));
    
    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Lazy expiry should trigger here
    EXPECT_FALSE(dict.exists("key1"));
}

TEST(ExpirationTest, ActiveExpirationDeletesBackground) {
    Dict dict;
    dict.set("key1", InfernoObject::create("val1"));
    dict.set("key2", InfernoObject::create("val2"));
    
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    dict.setExpire("key1", now + 50);
    dict.setExpire("key2", now + 5000); // Expiry far in future
    
    EXPECT_EQ(dict.size(), 2);
    
    // Wait for key1 to expire logically
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Run active expiry
    dict.activeExpireCheck();
    
    // key1 should be gone, key2 should still be there
    // We check size directly instead of exists() to ensure activeExpireCheck worked
    // (exists() would trigger lazy expire)
    EXPECT_EQ(dict.size(), 1);
    
    // Double check
    EXPECT_TRUE(dict.exists("key2"));
}

TEST(ExpirationTest, TTLAndPTTL) {
    Dict dict;
    dict.set("key1", InfernoObject::create("val1"));
    
    // No TTL initially
    EXPECT_EQ(dict.getTTL("key1"), -1);
    
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    dict.setExpire("key1", now + 5000); // 5 seconds
    
    int64_t ttl = dict.getTTL("key1");
    EXPECT_GT(ttl, 0);
    EXPECT_LE(ttl, 5000);
}

TEST(ExpirationTest, RemoveExpirePersist) {
    Dict dict;
    dict.set("key1", InfernoObject::create("val1"));
    
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    dict.setExpire("key1", now + 50);
    
    EXPECT_TRUE(dict.removeExpire("key1"));
    EXPECT_EQ(dict.getTTL("key1"), -1);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Should still exist because we removed the expiry
    EXPECT_TRUE(dict.exists("key1"));
}
