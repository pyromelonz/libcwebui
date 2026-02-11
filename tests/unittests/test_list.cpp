#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "linked_list.h"
}

typedef struct {
    char name[100];
    char value[100];
} list_test_ele;

static list_test_ele* gen_ele(const char* name, const char* value) {
    list_test_ele* ret = (list_test_ele*)malloc(sizeof(list_test_ele));
    snprintf(ret->name, sizeof(ret->name), "%s", name);
    snprintf(ret->value, sizeof(ret->value), "%s", value);
    return ret;
}

static int seeker(const void *el, const void *indicator) {
    list_test_ele *ele = (list_test_ele*)el;
    if (strcmp(ele->name, (const char*)indicator) == 0) {
        return 1;
    }
    return 0;
}

class LinkedListTest : public ::testing::Test {
protected:
    list_t list;

    void SetUp() override {
        ws_list_init(&list);
        ws_list_attributes_seeker(&list, seeker);
    }

    void TearDown() override {
        // Free remaining elements
        while (ws_list_size(&list) > 0) {
            void* ele = ws_list_extract_at(&list, 0);
            free(ele);
        }
    }
};

TEST_F(LinkedListTest, InitiallyEmpty) {
    EXPECT_EQ(ws_list_size(&list), 0);
    EXPECT_NE(ws_list_empty(&list), 0);
}

TEST_F(LinkedListTest, AppendAndSize) {
    list_test_ele* e0 = gen_ele("ele0", "value0");
    list_test_ele* e1 = gen_ele("ele1", "value1");

    ws_list_append(&list, e0, 0);
    EXPECT_EQ(ws_list_size(&list), 1);

    ws_list_append(&list, e1, 0);
    EXPECT_EQ(ws_list_size(&list), 2);

    EXPECT_EQ(ws_list_empty(&list), 0);
}

TEST_F(LinkedListTest, AppendFirst) {
    list_test_ele* e0 = gen_ele("ele0", "value0");
    list_test_ele* e1 = gen_ele("ele1", "value1");
    list_test_ele* e2 = gen_ele("ele2", "value2");

    ws_list_append(&list, e0, 0);
    ws_list_append(&list, e1, WS_LIST_APPEND_FIRST);
    ws_list_append(&list, e2, WS_LIST_APPEND_FIRST);

    // Order should be: e2, e1, e0
    list_test_ele* first = (list_test_ele*)ws_list_get_at(&list, 0);
    EXPECT_STREQ(first->name, "ele2");

    list_test_ele* second = (list_test_ele*)ws_list_get_at(&list, 1);
    EXPECT_STREQ(second->name, "ele1");

    list_test_ele* third = (list_test_ele*)ws_list_get_at(&list, 2);
    EXPECT_STREQ(third->name, "ele0");
}

TEST_F(LinkedListTest, GetAt) {
    list_test_ele* e0 = gen_ele("ele0", "value0");
    list_test_ele* e1 = gen_ele("ele1", "value1");
    list_test_ele* e2 = gen_ele("ele2", "value2");

    ws_list_append(&list, e0, 0);
    ws_list_append(&list, e1, 0);
    ws_list_append(&list, e2, 0);

    list_test_ele* ele = (list_test_ele*)ws_list_get_at(&list, 0);
    EXPECT_STREQ(ele->name, "ele0");

    ele = (list_test_ele*)ws_list_get_at(&list, 1);
    EXPECT_STREQ(ele->name, "ele1");

    ele = (list_test_ele*)ws_list_get_at(&list, 2);
    EXPECT_STREQ(ele->name, "ele2");
}

TEST_F(LinkedListTest, ExtractAt) {
    list_test_ele* e0 = gen_ele("ele0", "value0");
    list_test_ele* e1 = gen_ele("ele1", "value1");
    list_test_ele* e2 = gen_ele("ele2", "value2");

    ws_list_append(&list, e0, 0);
    ws_list_append(&list, e1, 0);
    ws_list_append(&list, e2, 0);

    EXPECT_EQ(ws_list_size(&list), 3);

    list_test_ele* extracted = (list_test_ele*)ws_list_extract_at(&list, 1);
    EXPECT_STREQ(extracted->name, "ele1");
    free(extracted);

    EXPECT_EQ(ws_list_size(&list), 2);

    // Verify remaining elements
    list_test_ele* first = (list_test_ele*)ws_list_get_at(&list, 0);
    EXPECT_STREQ(first->name, "ele0");

    list_test_ele* second = (list_test_ele*)ws_list_get_at(&list, 1);
    EXPECT_STREQ(second->name, "ele2");
}

TEST_F(LinkedListTest, Seek) {
    list_test_ele* e0 = gen_ele("ele0", "value0");
    list_test_ele* e1 = gen_ele("ele1", "value1");
    list_test_ele* e2 = gen_ele("ele2", "value2");

    ws_list_append(&list, e0, 0);
    ws_list_append(&list, e1, 0);
    ws_list_append(&list, e2, 0);

    list_test_ele* found = (list_test_ele*)ws_list_seek(&list, "ele1");
    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found->name, "ele1");
    EXPECT_STREQ(found->value, "value1");

    // Search for non-existent
    found = (list_test_ele*)ws_list_seek(&list, "nonexistent");
    EXPECT_EQ(found, nullptr);
}

TEST_F(LinkedListTest, Delete) {
    list_test_ele* e0 = gen_ele("ele0", "value0");
    list_test_ele* e1 = gen_ele("ele1", "value1");
    list_test_ele* e2 = gen_ele("ele2", "value2");

    ws_list_append(&list, e0, 0);
    ws_list_append(&list, e1, 0);
    ws_list_append(&list, e2, 0);

    EXPECT_EQ(ws_list_size(&list), 3);

    ws_list_delete(&list, e1);
    free(e1);

    EXPECT_EQ(ws_list_size(&list), 2);

    // Verify e1 is gone
    list_test_ele* found = (list_test_ele*)ws_list_seek(&list, "ele1");
    EXPECT_EQ(found, nullptr);

    // Verify others still there
    found = (list_test_ele*)ws_list_seek(&list, "ele0");
    EXPECT_NE(found, nullptr);

    found = (list_test_ele*)ws_list_seek(&list, "ele2");
    EXPECT_NE(found, nullptr);
}

TEST_F(LinkedListTest, Iterator) {
    list_test_ele* e0 = gen_ele("ele0", "value0");
    list_test_ele* e1 = gen_ele("ele1", "value1");
    list_test_ele* e2 = gen_ele("ele2", "value2");

    ws_list_append(&list, e0, 0);
    ws_list_append(&list, e1, 0);
    ws_list_append(&list, e2, 0);

    int count = 0;
    ws_list_iterator_start(&list);
    while (ws_list_iterator_hasnext(&list)) {
        list_test_ele* ele = (list_test_ele*)ws_list_iterator_next(&list);
        EXPECT_NE(ele, nullptr);
        count++;
    }
    ws_list_iterator_stop(&list);

    EXPECT_EQ(count, 3);
}

TEST_F(LinkedListTest, Destroy) {
    list_test_ele* e0 = gen_ele("ele0", "value0");
    list_test_ele* e1 = gen_ele("ele1", "value1");

    ws_list_append(&list, e0, 0);
    ws_list_append(&list, e1, 0);

    EXPECT_EQ(ws_list_size(&list), 2);

    // Note: ws_list_destroy doesn't free elements, just clears the list
    // We need to extract and free them first for proper cleanup
    while (ws_list_size(&list) > 0) {
        void* ele = ws_list_extract_at(&list, 0);
        free(ele);
    }

    EXPECT_EQ(ws_list_size(&list), 0);
    EXPECT_NE(ws_list_empty(&list), 0);
}
