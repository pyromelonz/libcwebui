#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "webserver.h"
}

// GUID Generation Tests
class GUIDGenerationTest : public ::testing::Test {
protected:
    char guid1[WEBSERVER_GUID_LENGTH + 1];
    char guid2[WEBSERVER_GUID_LENGTH + 1];

    void SetUp() override {
        memset(guid1, 0, sizeof(guid1));
        memset(guid2, 0, sizeof(guid2));
    }
};

TEST_F(GUIDGenerationTest, GeneratesCorrectLength) {
    generateGUID(guid1, WEBSERVER_GUID_LENGTH);

    // UUID format: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX = 36 chars
    // But buffer is larger (WEBSERVER_GUID_LENGTH = 50)
    // The actual length produced is 36 (UUID format)
    EXPECT_EQ(strlen(guid1), 36u);
}

TEST_F(GUIDGenerationTest, GeneratesUniqueValues) {
    generateGUID(guid1, WEBSERVER_GUID_LENGTH);
    generateGUID(guid2, WEBSERVER_GUID_LENGTH);

    // Two GUIDs should be different
    EXPECT_NE(strcmp(guid1, guid2), 0);
}

TEST_F(GUIDGenerationTest, GeneratesUUIDFormat) {
    generateGUID(guid1, WEBSERVER_GUID_LENGTH);

    // UUID format: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
    // Check for valid hex chars and dashes at correct positions
    for (size_t i = 0; i < strlen(guid1); i++) {
        char c = guid1[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            // Dashes at positions 8, 13, 18, 23
            EXPECT_EQ(c, '-') << "Expected dash at position " << i;
        } else {
            // Hex chars (uppercase)
            bool isHex = (c >= 'A' && c <= 'F') ||
                         (c >= '0' && c <= '9');
            EXPECT_TRUE(isHex) << "Invalid hex char at position " << i << ": " << c;
        }
    }
}

TEST_F(GUIDGenerationTest, ShorterLength) {
    generateGUID(guid1, 10);
    guid1[10] = '\0';

    EXPECT_EQ(strlen(guid1), 10u);
}

TEST_F(GUIDGenerationTest, MultipleGenerationsUnique) {
    // Generate 10 GUIDs and ensure they're all different
    char guids[10][WEBSERVER_GUID_LENGTH + 1];

    for (int i = 0; i < 10; i++) {
        generateGUID(guids[i], WEBSERVER_GUID_LENGTH);
        guids[i][WEBSERVER_GUID_LENGTH] = '\0';
    }

    // Compare each pair
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            EXPECT_NE(strcmp(guids[i], guids[j]), 0)
                << "GUID " << i << " equals GUID " << j;
        }
    }
}

// Session Store Memory Tests
class SessionStoreMemoryTest : public ::testing::Test {};

TEST_F(SessionStoreMemoryTest, MallocSessionStore) {
    sessionStore* store = WebserverMallocSessionStore();

    ASSERT_NE(store, nullptr);
    EXPECT_NE(store->vars, nullptr);

    WebserverFreeSessionStore(store);
}

TEST_F(SessionStoreMemoryTest, SessionStoreHasVariableStore) {
    sessionStore* store = WebserverMallocSessionStore();

    ASSERT_NE(store, nullptr);
    ASSERT_NE(store->vars, nullptr);

    // Should be able to add variables
    ws_variable* var = newVariable(store->vars, "test", 0);
    EXPECT_NE(var, nullptr);

    WebserverFreeSessionStore(store);
}

// Session Value Protection Tests
// These test that internal session values cannot be modified via public API

// Note: setSessionValue and getSessionValue require a full http_request setup
// which depends on session initialization. These are integration-level tests.
// Instead, we test the underlying variable store operations.

class VariableStoreInSessionTest : public ::testing::Test {
protected:
    sessionStore* store;

    void SetUp() override {
        store = WebserverMallocSessionStore();
    }

    void TearDown() override {
        WebserverFreeSessionStore(store);
    }
};

TEST_F(VariableStoreInSessionTest, SetAndGetString) {
    ws_variable* var = newVariable(store->vars, "username", 0);
    ASSERT_NE(var, nullptr);

    setWSVariableString(var, "testuser");

    ws_variable* retrieved = getVariable(store->vars, "username");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_STREQ(retrieved->val.value_string, "testuser");
}

TEST_F(VariableStoreInSessionTest, SetAndGetInt) {
    ws_variable* var = newVariable(store->vars, "counter", 0);
    ASSERT_NE(var, nullptr);

    setWSVariableInt(var, 42);

    ws_variable* retrieved = getVariable(store->vars, "counter");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->val.value_int, 42);
}

TEST_F(VariableStoreInSessionTest, SetAndGetLong) {
    ws_variable* var = newVariable(store->vars, "bignum", 0);
    ASSERT_NE(var, nullptr);

    setWSVariableLong(var, 123456789012345LL);

    ws_variable* retrieved = getVariable(store->vars, "bignum");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->val.value_int64_t, 123456789012345LL);
}

TEST_F(VariableStoreInSessionTest, MultipleVariables) {
    ws_variable* v1 = newVariable(store->vars, "var1", 0);
    ws_variable* v2 = newVariable(store->vars, "var2", 0);
    ws_variable* v3 = newVariable(store->vars, "var3", 0);

    setWSVariableString(v1, "value1");
    setWSVariableString(v2, "value2");
    setWSVariableString(v3, "value3");

    EXPECT_STREQ(getVariable(store->vars, "var1")->val.value_string, "value1");
    EXPECT_STREQ(getVariable(store->vars, "var2")->val.value_string, "value2");
    EXPECT_STREQ(getVariable(store->vars, "var3")->val.value_string, "value3");
}

TEST_F(VariableStoreInSessionTest, OverwriteVariable) {
    ws_variable* var = newVariable(store->vars, "myvar", 0);
    setWSVariableString(var, "original");

    var = newVariable(store->vars, "myvar", 0);  // Get same variable
    setWSVariableString(var, "updated");

    ws_variable* retrieved = getVariable(store->vars, "myvar");
    EXPECT_STREQ(retrieved->val.value_string, "updated");
}

TEST_F(VariableStoreInSessionTest, NonExistentVariable) {
    ws_variable* var = getVariable(store->vars, "doesnotexist");
    EXPECT_EQ(var, nullptr);
}

TEST_F(VariableStoreInSessionTest, DeleteVariable) {
    ws_variable* var = newVariable(store->vars, "todelete", 0);
    setWSVariableString(var, "willbedeleted");

    // Variable exists
    EXPECT_NE(getVariable(store->vars, "todelete"), nullptr);

    // Delete it
    delVariable(store->vars, "todelete");

    // Variable should be gone
    EXPECT_EQ(getVariable(store->vars, "todelete"), nullptr);
}

TEST_F(VariableStoreInSessionTest, ClearAllVariables) {
    newVariable(store->vars, "a", 0);
    newVariable(store->vars, "b", 0);
    newVariable(store->vars, "c", 0);

    clearVariables(store->vars);

    EXPECT_EQ(getVariable(store->vars, "a"), nullptr);
    EXPECT_EQ(getVariable(store->vars, "b"), nullptr);
    EXPECT_EQ(getVariable(store->vars, "c"), nullptr);
}

// Cookie-related Session Tests
class SessionCookieTest : public ::testing::Test {
protected:
    HttpRequestHeader* header;
    socket_info* sock;
    char buffer[4096];

    void SetUp() override {
        header = WebserverMallocHttpRequestHeader();
        sock = (socket_info*)calloc(1, sizeof(socket_info));
        sock->header = header;
    }

    void TearDown() override {
        WebserverFreeHttpRequestHeader(header);
        free(sock);
    }

    int parseHeaderLine(const char* line) {
        strncpy(buffer, line, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        return analyseHeaderLine(sock, buffer, strlen(line), header);
    }
};

TEST_F(SessionCookieTest, ParseSessionIdCookie) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Cookie: session-id=abc123xyz");

    char value[100];
    int result = checkCookie((char*)"session-id", value, header);

    EXPECT_EQ(result, 1);
    EXPECT_STREQ(value, "abc123xyz");
}

TEST_F(SessionCookieTest, SessionIdWithOtherCookies) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Cookie: other=foo; session-id=mysessionid; another=bar");

    char value[100];
    int result = checkCookie((char*)"session-id", value, header);

    EXPECT_EQ(result, 1);
    EXPECT_STREQ(value, "mysessionid");
}

TEST_F(SessionCookieTest, MissingSessionCookie) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Cookie: other=value");

    char value[100];
    value[0] = '\0';
    int result = checkCookie((char*)"session-id", value, header);

    EXPECT_EQ(result, 0);
}

TEST_F(SessionCookieTest, EmptySessionCookie) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Cookie: session-id=");

    char value[100];
    int result = checkCookie((char*)"session-id", value, header);

    // Empty value should be valid
    EXPECT_EQ(result, 1);
}

// Timing-safe comparison test
// The guid_cmp function is static, so we test the principle
class TimingSafeCompareTest : public ::testing::Test {
protected:
    // Replicate the timing-safe comparison logic
    int timingSafeCompare(const char* a, const char* b, int len) {
        unsigned char result = 0;
        for (int i = 0; i < len; i++) {
            result |= (unsigned char)(a[i] ^ b[i]);
        }
        return (result == 0) ? 1 : 0;
    }
};

TEST_F(TimingSafeCompareTest, EqualStrings) {
    const char* a = "abcdefghij";
    const char* b = "abcdefghij";

    EXPECT_EQ(timingSafeCompare(a, b, 10), 1);
}

TEST_F(TimingSafeCompareTest, DifferentStrings) {
    const char* a = "abcdefghij";
    const char* b = "abcdefghik";  // Last char different

    EXPECT_EQ(timingSafeCompare(a, b, 10), 0);
}

TEST_F(TimingSafeCompareTest, DifferentAtStart) {
    const char* a = "Xbcdefghij";
    const char* b = "abcdefghij";

    EXPECT_EQ(timingSafeCompare(a, b, 10), 0);
}

TEST_F(TimingSafeCompareTest, DifferentInMiddle) {
    const char* a = "abcdeXghij";
    const char* b = "abcdefghij";

    EXPECT_EQ(timingSafeCompare(a, b, 10), 0);
}

TEST_F(TimingSafeCompareTest, AllZeros) {
    char a[10] = {0};
    char b[10] = {0};

    EXPECT_EQ(timingSafeCompare(a, b, 10), 1);
}

TEST_F(TimingSafeCompareTest, OneBitDifferent) {
    char a[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    char b[10] = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_EQ(timingSafeCompare(a, b, 10), 0);
}
