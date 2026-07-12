#include <gtest/gtest.h>
#include "../src/server/pubsub.h"
#include <vector>
#include <string>

using namespace inferno::server;

class PubSubTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset singleton state if needed (not easily doable without adding a reset method,
        // so we just use unique channels for each test)
    }
};

TEST_F(PubSubTest, BasicPublishSubscribe) {
    auto& ps = PubSubManager::instance();
    
    // Track messages sent
    std::vector<std::pair<int, std::string>> sent_messages;
    
    ps.setSendCallback([&](int fd, const std::string& msg) {
        sent_messages.push_back({fd, msg});
    });
    
    // Subscribe two clients to "news"
    int fd1 = 101;
    int fd2 = 102;
    
    EXPECT_EQ(ps.subscribe(fd1, "news"), 1);
    EXPECT_EQ(ps.subscribe(fd2, "news"), 1);
    
    // Subscribe client 1 to "sports"
    EXPECT_EQ(ps.subscribe(fd1, "sports"), 2);
    
    // Publish to news
    EXPECT_EQ(ps.publish("news", "hello world"), 2);
    EXPECT_EQ(sent_messages.size(), 2); // Both should receive
    
    sent_messages.clear();
    
    // Publish to sports
    EXPECT_EQ(ps.publish("sports", "goal"), 1);
    EXPECT_EQ(sent_messages.size(), 1);
    EXPECT_EQ(sent_messages[0].first, fd1); // Only fd1 should receive
}

TEST_F(PubSubTest, Unsubscribe) {
    auto& ps = PubSubManager::instance();
    
    int fd3 = 103;
    ps.subscribe(fd3, "tech");
    
    // Publish should reach fd3
    EXPECT_EQ(ps.publish("tech", "msg1"), 1);
    
    // Unsubscribe
    EXPECT_EQ(ps.unsubscribe(fd3, "tech"), 0);
    
    // Publish should not reach anyone
    EXPECT_EQ(ps.publish("tech", "msg2"), 0);
}

TEST_F(PubSubTest, UnsubscribeAllOnDisconnect) {
    auto& ps = PubSubManager::instance();
    
    int fd4 = 104;
    ps.subscribe(fd4, "ch1");
    ps.subscribe(fd4, "ch2");
    ps.subscribe(fd4, "ch3");
    
    EXPECT_EQ(ps.publish("ch1", "m"), 1);
    
    // Simulate disconnect
    ps.unsubscribeAll(fd4);
    
    EXPECT_EQ(ps.publish("ch1", "m"), 0);
    EXPECT_EQ(ps.publish("ch2", "m"), 0);
    EXPECT_EQ(ps.publish("ch3", "m"), 0);
}
