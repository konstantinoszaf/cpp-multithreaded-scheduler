#include "scheduler/ring_buffer.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace scheduler::queue;

TEST(RingBuffer, IsInitiallyEmpty) {
    RingBuffer<int> buffer{1};
    ASSERT_TRUE(buffer.empty());
    ASSERT_FALSE(buffer.full());
}

TEST(RingBuffer, IsNotEmptyButFullAfterPush) {
    RingBuffer<int> buffer{1};

    bool res = buffer.push_back(1);

    ASSERT_TRUE(res);
    ASSERT_FALSE(buffer.empty());
    ASSERT_TRUE(buffer.full());
}

TEST(RingBuffer, PushOnFullBufferShouldFail) {
    RingBuffer<int> buffer{0};

    bool res = buffer.push_back(1);

    ASSERT_FALSE(res);
    ASSERT_TRUE(buffer.empty());
    ASSERT_TRUE(buffer.full());
}

TEST(RingBuffer, PushAndGetElementFromBack) {
    RingBuffer<int> buffer{1};

    bool res = buffer.push_back(1);

    ASSERT_TRUE(res);
    EXPECT_EQ(1, buffer.back());
    ASSERT_TRUE(buffer.full());
}

TEST(RingBuffer, PushAndPopElementsFromBack) {
    RingBuffer<int> buffer{2};

    buffer.push_back(1);
    buffer.push_back(2);

    buffer.pop_back();
    EXPECT_EQ(1, buffer.back());
    EXPECT_EQ(1, buffer.size());
}

TEST(RingBuffer, PushAndPopElementsFromFront) {
    RingBuffer<int> buffer{2};

    buffer.push_back(1);
    buffer.push_back(2);

    ASSERT_EQ(1, buffer.front());

    buffer.pop_front();
    ASSERT_EQ(2, buffer.front());
    ASSERT_EQ(2, buffer.back());
}

TEST(RingBuffer, FillAndDrain) {
    RingBuffer<int> buffer{1024};

    // Fill
    for (int i = 0; i < 1024; ++i) {
        ASSERT_TRUE(buffer.push_back(i));
        EXPECT_EQ(i + 1, buffer.size());
    }

    EXPECT_TRUE(buffer.full());
    EXPECT_FALSE(buffer.empty());
    EXPECT_EQ(1024u, buffer.size());
    EXPECT_EQ(1023, buffer.back());
    EXPECT_EQ(0, buffer.front());

    // Drain
    for (int i = 0; i < 1024; ++i) {
        EXPECT_FALSE(buffer.empty());
        EXPECT_EQ(i, buffer.front());
        EXPECT_TRUE(buffer.pop_front());
    }

    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(0u, buffer.size());
}

TEST(RingBuffer, WrapAroundFrontBack) {
    RingBuffer<int> buffer{8}; // small to force wrap

    int next_value = 0;

    // Do many ops to wrap around multiple times
    for (int iter = 0; iter < 10000; ++iter) {
        // Keep it about half full
        if (!buffer.full()) {
            ASSERT_TRUE(buffer.push_back(next_value));
            ++next_value;
        }

        if (!buffer.empty()) {
            int front = buffer.front();
            ASSERT_TRUE(buffer.pop_front());
            // we don't check exact sequence here, just that it doesn't crash
            (void)front;
        }
    }
}
