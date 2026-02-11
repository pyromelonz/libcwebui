#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "webserver.h"
}

class VariableTest : public ::testing::Test {
protected:
    ws_variable* var;

    void SetUp() override {
        var = nullptr;
    }

    void TearDown() override {
        if (var != nullptr) {
            freeWSVariable(var);
        }
    }
};

// newWSVariable tests
TEST_F(VariableTest, CreateWithName) {
    var = newWSVariable("test_var");
    ASSERT_NE(var, nullptr);
    EXPECT_STREQ(var->name, "test_var");
    EXPECT_EQ(var->type, VAR_TYPE_EMPTY);
}

TEST_F(VariableTest, CreateWithNullName) {
    var = newWSVariable(nullptr);
    ASSERT_NE(var, nullptr);
    EXPECT_EQ(var->name, nullptr);
}

TEST_F(VariableTest, CreateWithEmptyName) {
    var = newWSVariable("");
    ASSERT_NE(var, nullptr);
    EXPECT_STREQ(var->name, "");
}

// String variable tests
TEST_F(VariableTest, SetStringValue) {
    var = newWSVariable("str_var");
    setWSVariableString(var, "hello world");

    EXPECT_EQ(var->type, VAR_TYPE_STRING);

    char buffer[100];
    getWSVariableString(var, buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "hello world");
}

TEST_F(VariableTest, SetStringNull) {
    var = newWSVariable("str_var");
    setWSVariableString(var, nullptr);

    EXPECT_EQ(var->type, VAR_TYPE_STRING);
    // Should create a placeholder string
    EXPECT_NE(var->val.value_string, nullptr);
}

TEST_F(VariableTest, SetStringEmpty) {
    var = newWSVariable("str_var");
    setWSVariableString(var, "");

    char buffer[100];
    getWSVariableString(var, buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "");
}

TEST_F(VariableTest, OverwriteString) {
    var = newWSVariable("str_var");
    setWSVariableString(var, "first");
    setWSVariableString(var, "second");

    char buffer[100];
    getWSVariableString(var, buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "second");
}

// Integer variable tests
TEST_F(VariableTest, SetIntValue) {
    var = newWSVariable("int_var");
    setWSVariableInt(var, 42);

    EXPECT_EQ(var->type, VAR_TYPE_INT);
    EXPECT_EQ(getWSVariableInt(var), 42);
}

TEST_F(VariableTest, SetIntNegative) {
    var = newWSVariable("int_var");
    setWSVariableInt(var, -100);

    EXPECT_EQ(getWSVariableInt(var), -100);
}

TEST_F(VariableTest, SetIntZero) {
    var = newWSVariable("int_var");
    setWSVariableInt(var, 0);

    EXPECT_EQ(getWSVariableInt(var), 0);
}

TEST_F(VariableTest, GetIntAsString) {
    var = newWSVariable("int_var");
    setWSVariableInt(var, 12345);

    char buffer[100];
    getWSVariableString(var, buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "12345");
}

// ULong variable tests
TEST_F(VariableTest, SetULongValue) {
    var = newWSVariable("ulong_var");
    setWSVariableULong(var, 12345678901234ULL);

    EXPECT_EQ(var->type, VAR_TYPE_ULONG);
    EXPECT_EQ(getWSVariableULong(var), 12345678901234ULL);
}

TEST_F(VariableTest, SetULongZero) {
    var = newWSVariable("ulong_var");
    setWSVariableULong(var, 0);

    EXPECT_EQ(getWSVariableULong(var), 0ULL);
}

TEST_F(VariableTest, SetULongMax) {
    var = newWSVariable("ulong_var");
    setWSVariableULong(var, UINT64_MAX);

    EXPECT_EQ(getWSVariableULong(var), UINT64_MAX);
}

// Long variable tests
TEST_F(VariableTest, SetLongValue) {
    var = newWSVariable("long_var");
    setWSVariableLong(var, -12345678901234LL);

    EXPECT_EQ(var->type, VAR_TYPE_LONG);
    EXPECT_EQ(getWSVariableLong(var), -12345678901234LL);
}

TEST_F(VariableTest, SetLongPositive) {
    var = newWSVariable("long_var");
    setWSVariableLong(var, 9876543210LL);

    EXPECT_EQ(getWSVariableLong(var), 9876543210LL);
}

// Type conversion tests
TEST_F(VariableTest, GetIntFromString) {
    var = newWSVariable("var");
    setWSVariableString(var, "999");

    EXPECT_EQ(getWSVariableInt(var), 999);
}

TEST_F(VariableTest, GetULongFromString) {
    var = newWSVariable("var");
    setWSVariableString(var, "123456789012");

    EXPECT_EQ(getWSVariableULong(var), 123456789012ULL);
}

TEST_F(VariableTest, GetLongFromString) {
    var = newWSVariable("var");
    setWSVariableString(var, "-123456789012");

    EXPECT_EQ(getWSVariableLong(var), -123456789012LL);
}

TEST_F(VariableTest, GetIntFromULong) {
    var = newWSVariable("var");
    setWSVariableULong(var, 42);

    EXPECT_EQ(getWSVariableInt(var), 42);
}

TEST_F(VariableTest, GetIntFromEmpty) {
    var = newWSVariable("var");

    EXPECT_EQ(getWSVariableInt(var), 0);
}

TEST_F(VariableTest, GetIntFromNull) {
    EXPECT_EQ(getWSVariableInt(nullptr), 0);
}

// Array tests
class VariableArrayTest : public ::testing::Test {
protected:
    ws_variable* arr;

    void SetUp() override {
        arr = newWSArray("test_array");
    }

    void TearDown() override {
        if (arr != nullptr) {
            freeWSVariable(arr);
        }
    }
};

TEST_F(VariableArrayTest, CreateArray) {
    ASSERT_NE(arr, nullptr);
    EXPECT_EQ(arr->type, VAR_TYPE_ARRAY);
}

TEST_F(VariableArrayTest, AddToArray) {
    ws_variable* elem = addWSVariableArray(arr, "elem1", 0);
    ASSERT_NE(elem, nullptr);
    setWSVariableString(elem, "value1");

    ws_variable* retrieved = getWSVariableArray(arr, "elem1");
    ASSERT_NE(retrieved, nullptr);

    char buffer[100];
    getWSVariableString(retrieved, buffer, sizeof(buffer));
    EXPECT_STREQ(buffer, "value1");
}

TEST_F(VariableArrayTest, AddMultipleElements) {
    addWSVariableArray(arr, "a", 0);
    addWSVariableArray(arr, "b", 0);
    addWSVariableArray(arr, "c", 0);

    EXPECT_NE(getWSVariableArray(arr, "a"), nullptr);
    EXPECT_NE(getWSVariableArray(arr, "b"), nullptr);
    EXPECT_NE(getWSVariableArray(arr, "c"), nullptr);
}

TEST_F(VariableArrayTest, GetNonExistentElement) {
    ws_variable* elem = getWSVariableArray(arr, "nonexistent");
    EXPECT_EQ(elem, nullptr);
}

TEST_F(VariableArrayTest, DeleteFromArray) {
    addWSVariableArray(arr, "elem1", 0);
    EXPECT_NE(getWSVariableArray(arr, "elem1"), nullptr);

    delWSVariableArray(arr, "elem1");
    EXPECT_EQ(getWSVariableArray(arr, "elem1"), nullptr);
}

TEST_F(VariableArrayTest, ArrayIteration) {
    ws_variable* e1 = addWSVariableArray(arr, "first", 0);
    ws_variable* e2 = addWSVariableArray(arr, "second", 0);
    setWSVariableInt(e1, 1);
    setWSVariableInt(e2, 2);

    int count = 0;
    ws_variable* elem = getWSVariableArrayFirst(arr);
    while (elem != nullptr) {
        count++;
        elem = getWSVariableArrayNext(arr);
    }

    EXPECT_EQ(count, 2);
}

TEST_F(VariableArrayTest, GetByIndex) {
    ws_variable* e0 = addWSVariableArray(arr, "idx0", 0);
    ws_variable* e1 = addWSVariableArray(arr, "idx1", 0);
    setWSVariableInt(e0, 100);
    setWSVariableInt(e1, 200);

    ws_variable* found = getWSVariableArrayIndex(arr, 0);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(getWSVariableInt(found), 100);

    found = getWSVariableArrayIndex(arr, 1);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(getWSVariableInt(found), 200);
}

TEST_F(VariableArrayTest, GetByIndexOutOfBounds) {
    addWSVariableArray(arr, "elem", 0);

    ws_variable* found = getWSVariableArrayIndex(arr, 5);
    EXPECT_EQ(found, nullptr);
}

TEST_F(VariableArrayTest, AddByIndex) {
    ws_variable* e = addWSVariableArrayIndex(arr, 2);
    ASSERT_NE(e, nullptr);
    setWSVariableString(e, "at index 2");

    // Should also create elements at index 0 and 1
    EXPECT_NE(getWSVariableArrayIndex(arr, 0), nullptr);
    EXPECT_NE(getWSVariableArrayIndex(arr, 1), nullptr);
    EXPECT_NE(getWSVariableArrayIndex(arr, 2), nullptr);
}

// Null pointer safety tests
TEST_F(VariableArrayTest, SetNullVariable) {
    // Should not crash
    setWSVariableString(nullptr, "test");
    setWSVariableInt(nullptr, 42);
    setWSVariableULong(nullptr, 123);
    setWSVariableLong(nullptr, -456);
}

TEST_F(VariableArrayTest, GetFromNullArray) {
    EXPECT_EQ(getWSVariableArray(nullptr, "test"), nullptr);
    EXPECT_EQ(addWSVariableArray(nullptr, "test", 0), nullptr);
}

TEST_F(VariableArrayTest, ArrayOnNonArray) {
    ws_variable* v = newWSVariable("not_array");
    setWSVariableInt(v, 42);

    // These should return nullptr or do nothing
    EXPECT_EQ(getWSVariableArray(v, "test"), nullptr);
    EXPECT_EQ(getWSVariableArrayFirst(v), nullptr);
    EXPECT_EQ(getWSVariableArrayIndex(v, 0), nullptr);

    freeWSVariable(v);
}

// Variable size tests
TEST_F(VariableArrayTest, GetVariableSize) {
    ws_variable* v = newWSVariable("size_test");
    SIZE_TYPE size1 = getWSVariableSize(v);
    EXPECT_GT(size1, 0u);

    setWSVariableString(v, "this is a test string");
    SIZE_TYPE size2 = getWSVariableSize(v);
    EXPECT_GT(size2, size1);

    freeWSVariable(v);
}

TEST_F(VariableArrayTest, GetNullVariableSize) {
    EXPECT_EQ(getWSVariableSize(nullptr), 0u);
}
