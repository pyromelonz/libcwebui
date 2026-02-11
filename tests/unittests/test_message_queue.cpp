#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

extern "C" {
#include "webserver.h"
}

// Message Queue Tests
class MessageQueueTest : public ::testing::Test {
protected:
    ws_MessageQueue* queue;

    void SetUp() override {
        queue = ws_createMessageQueue();
    }

    void TearDown() override {
        ws_destroyMessageQueue(queue);
    }
};

TEST_F(MessageQueueTest, CreateQueue) {
    ASSERT_NE(queue, nullptr);
}

TEST_F(MessageQueueTest, PushAndPopSingleElement) {
    int value = 42;
    int* ptr = &value;

    ws_pushQueue(queue, ptr);

    // Create a thread to pop (since popQueue blocks on semaphore)
    std::atomic<int*> result{nullptr};
    std::thread t([this, &result]() {
        result = (int*)ws_popQueue(queue);
    });

    t.join();
    EXPECT_EQ(result.load(), ptr);
    EXPECT_EQ(*result.load(), 42);
}

TEST_F(MessageQueueTest, FIFOOrder) {
    int values[3] = {1, 2, 3};

    ws_pushQueue(queue, &values[0]);
    ws_pushQueue(queue, &values[1]);
    ws_pushQueue(queue, &values[2]);

    std::vector<int*> results;
    std::thread t([this, &results]() {
        results.push_back((int*)ws_popQueue(queue));
        results.push_back((int*)ws_popQueue(queue));
        results.push_back((int*)ws_popQueue(queue));
    });

    t.join();

    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(*results[0], 1);
    EXPECT_EQ(*results[1], 2);
    EXPECT_EQ(*results[2], 3);
}

TEST_F(MessageQueueTest, MultipleProducers) {
    std::atomic<int> counter{0};
    int values[10];

    // Initialize values
    for (int i = 0; i < 10; i++) {
        values[i] = i;
    }

    // Start consumer thread
    std::thread consumer([this, &counter]() {
        for (int i = 0; i < 10; i++) {
            ws_popQueue(queue);
            counter++;
        }
    });

    // Two producer threads
    std::thread producer1([this, &values]() {
        for (int i = 0; i < 5; i++) {
            ws_pushQueue(queue, &values[i]);
        }
    });

    std::thread producer2([this, &values]() {
        for (int i = 5; i < 10; i++) {
            ws_pushQueue(queue, &values[i]);
        }
    });

    producer1.join();
    producer2.join();
    consumer.join();

    EXPECT_EQ(counter.load(), 10);
}

TEST_F(MessageQueueTest, PopBlocksUntilPush) {
    std::atomic<bool> popped{false};
    std::atomic<bool> started{false};
    int value = 123;

    // Consumer will block
    std::thread consumer([this, &popped, &started]() {
        started = true;
        ws_popQueue(queue);
        popped = true;
    });

    // Wait for consumer to start
    while (!started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Consumer should be blocked
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(popped.load());

    // Now push
    ws_pushQueue(queue, &value);

    consumer.join();
    EXPECT_TRUE(popped.load());
}

TEST_F(MessageQueueTest, AllocatedData) {
    // Test with dynamically allocated data (typical use case)
    char* msg = (char*)WebserverMalloc(100);
    strcpy(msg, "test message");

    ws_pushQueue(queue, msg);

    std::thread t([this]() {
        char* received = (char*)ws_popQueue(queue);
        EXPECT_STREQ(received, "test message");
        WebserverFree(received);
    });

    t.join();
}

TEST_F(MessageQueueTest, NullPointer) {
    // Test that null pointers can be queued
    ws_pushQueue(queue, nullptr);

    std::thread t([this]() {
        void* result = ws_popQueue(queue);
        EXPECT_EQ(result, nullptr);
    });

    t.join();
}

TEST_F(MessageQueueTest, LargeNumberOfMessages) {
    const int NUM_MESSAGES = 1000;
    std::vector<int> values(NUM_MESSAGES);
    std::atomic<int> received{0};

    for (int i = 0; i < NUM_MESSAGES; i++) {
        values[i] = i;
    }

    // Consumer
    std::thread consumer([this, &received, NUM_MESSAGES]() {
        for (int i = 0; i < NUM_MESSAGES; i++) {
            ws_popQueue(queue);
            received++;
        }
    });

    // Producer
    std::thread producer([this, &values, NUM_MESSAGES]() {
        for (int i = 0; i < NUM_MESSAGES; i++) {
            ws_pushQueue(queue, &values[i]);
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(received.load(), NUM_MESSAGES);
}

// Linked List in Queue Context Tests
class LinkedListQueueTest : public ::testing::Test {};

TEST_F(LinkedListQueueTest, AppendAndExtract) {
    list_t list;
    ws_list_init(&list);

    int values[3] = {10, 20, 30};

    ws_list_append(&list, &values[0], 0);
    ws_list_append(&list, &values[1], 0);
    ws_list_append(&list, &values[2], 0);

    EXPECT_EQ(ws_list_size(&list), 3u);

    // Extract in FIFO order
    int* v1 = (int*)ws_list_extract_at(&list, 0);
    EXPECT_EQ(*v1, 10);

    int* v2 = (int*)ws_list_extract_at(&list, 0);
    EXPECT_EQ(*v2, 20);

    int* v3 = (int*)ws_list_extract_at(&list, 0);
    EXPECT_EQ(*v3, 30);

    EXPECT_EQ(ws_list_size(&list), 0u);

    ws_list_destroy(&list);
}

TEST_F(LinkedListQueueTest, EmptyCheck) {
    list_t list;
    ws_list_init(&list);

    EXPECT_EQ(ws_list_empty(&list), 1);

    int value = 1;
    ws_list_append(&list, &value, 0);

    EXPECT_EQ(ws_list_empty(&list), 0);

    ws_list_extract_at(&list, 0);

    EXPECT_EQ(ws_list_empty(&list), 1);

    ws_list_destroy(&list);
}
